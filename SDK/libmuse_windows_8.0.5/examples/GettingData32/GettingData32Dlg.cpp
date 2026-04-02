
// GettingData32Dlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "GettingData32.h"
#include "GettingData32Dlg.h"
#include "afxdialogex.h"
#include <sstream>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>

#include <winrt/Windows.Management.h>
#include <winrt/Windows.Management.Deployment.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Refresh rate for the data display on the screen (Hz)
#define REFRESH_RATE 60
// Decimal precision for displayed data
#define DATA_PRECISION 4

#define MUSE_LIST_CHANGED (WM_USER + 100)
#define RECEIVE_CONNECTION_PACKET (WM_USER + 101)
#define RECEIVE_MUSE_DATA_PACKET (WM_USER + 102)
#define RECEIVE_MUSE_ARTIFACT_PACKET (WM_USER + 103)
#define UPDATE_UI (WM_USER + 104)
#define RECEIVE_LOG (WM_USER + 105)

#pragma comment(lib, "libmuse-wrt")

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::ApplicationModel::Core;

// CGettingData32Dlg dialog

CGettingData32Dlg::CGettingData32Dlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_GETTINGDATA32_DIALOG, pParent),
    current_data_type_(MuseDataPacketType::ACCELEROMETER),
    my_muse_model_(MuseModel::MU_02)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CGettingData32Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_MUSE, combo_muse_list_);
    DDX_Control(pDX, IDC_COMBO_DATA_TYPE, combo_data_type_);
    DDX_Control(pDX, IDC_LABEL1, line_1_label);
    DDX_Control(pDX, IDC_LABEL1_DATA, line_1_data);
    DDX_Control(pDX, IDC_LABEL2, line_2_label);
    DDX_Control(pDX, IDC_LABEL2_DATA, line_2_data);
    DDX_Control(pDX, IDC_LABEL3, line_3_label);
    DDX_Control(pDX, IDC_LABEL3_DATA, line_3_data);
    DDX_Control(pDX, IDC_LABEL4, line_4_label);
    DDX_Control(pDX, IDC_LABEL4_DATA, line_4_data);
    DDX_Control(pDX, IDC_LABEL5, line_5_label);
    DDX_Control(pDX, IDC_LABEL5_DATA, line_5_data);
    DDX_Control(pDX, IDC_LABEL6, line_6_label);
    DDX_Control(pDX, IDC_LABEL6_DATA, line_6_data);
    DDX_Control(pDX, IDC_STATUS, connection_status);
    DDX_Control(pDX, IDC_VERSION, version);
}

BEGIN_MESSAGE_MAP(CGettingData32Dlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_CLOSE()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_REFRESH, &CGettingData32Dlg::OnBnClickedRefresh)
    ON_BN_CLICKED(IDC_CONNECT, &CGettingData32Dlg::OnBnClickedConnect)
    ON_BN_CLICKED(IDC_DISCONNECT, &CGettingData32Dlg::OnBnClickedDisconnect)
    ON_CBN_SELCHANGE(IDC_COMBO_DATA_TYPE, &CGettingData32Dlg::OnCbnSelchangeComboDataType)
    ON_MESSAGE(MUSE_LIST_CHANGED, &CGettingData32Dlg::OnMuseListChanged)
    ON_MESSAGE(RECEIVE_CONNECTION_PACKET, &CGettingData32Dlg::OnReceiveConnectionPacket)
    ON_MESSAGE(RECEIVE_MUSE_ARTIFACT_PACKET, &CGettingData32Dlg::OnReceiveMuseArtifactPacket)
    ON_MESSAGE(RECEIVE_MUSE_DATA_PACKET, &CGettingData32Dlg::OnReceiveMuseDataPacket)
    ON_MESSAGE(UPDATE_UI, &CGettingData32Dlg::OnUpdateUI)
    ON_MESSAGE(RECEIVE_LOG, &CGettingData32Dlg::OnReceiveLog)
END_MESSAGE_MAP()

// CGettingData32Dlg message handlers
static HWND hwnd;

BOOL CGettingData32Dlg::OnInitDialog()
{
    OutputDebugString(L"CGettingData32Dlg::OnInitDialog\n");
    CDialogEx::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    SetWindowText(L"GettingData32");

    hwnd = m_hWnd;
    init_data_type_combobox();
    queue_ui_update();

    manager_ = MuseManagerWindows::get_instance();
    muse_listener_ = std::make_shared<MuseListenerWin>(this);
    connection_listener_ = std::make_shared<ConnectionListener>(this);
    data_listener_ = std::make_shared<DataListener>(this);
    log_listener_ = std::make_shared<LogListenerWin>(this);
    LogManager::instance()->set_log_listener(log_listener_);

    manager_->set_muse_listener(muse_listener_);
    manager_->remove_from_list_after(0);
    is_bluetooth_enabled_.store(false);
    check_bluetooth_enabled();
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CGettingData32Dlg::OnClose()
{
    OutputDebugString(L"CGettingData32Dlg::OnClose\n");
    if (closing_) {
        CDialogEx::OnClose();
        return;
    }
    CloseAsync();
}

void CGettingData32Dlg::CloseAsync()
{
    closing_ = true;
    GetSystemMenu(false)->EnableMenuItem(SC_CLOSE, MF_DISABLED);
    disconnect_async().then([] {
        ::PostMessage(hwnd, WM_CLOSE, NULL, NULL);
    });
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CGettingData32Dlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

void CGettingData32Dlg::init_data_type_combobox() {
    add_type("ACCELEROMETER", MuseDataPacketType::ACCELEROMETER);
    add_type("GYRO", MuseDataPacketType::GYRO);
    add_type("EEG", MuseDataPacketType::EEG);
    add_type("PPG", MuseDataPacketType::PPG);
    add_type("OPTICS", MuseDataPacketType::OPTICS);
    add_type("BATTERY", MuseDataPacketType::BATTERY);
    add_type("DRL_REF", MuseDataPacketType::DRL_REF);
    add_type("ALPHA_ABSOLUTE", MuseDataPacketType::ALPHA_ABSOLUTE);
    add_type("BETA_ABSOLUTE", MuseDataPacketType::BETA_ABSOLUTE);
    add_type("DELTA_ABSOLUTE", MuseDataPacketType::DELTA_ABSOLUTE);
    add_type("THETA_ABSOLUTE", MuseDataPacketType::THETA_ABSOLUTE);
    add_type("GAMMA_ABSOLUTE", MuseDataPacketType::GAMMA_ABSOLUTE);
    add_type("ALPHA_RELATIVE", MuseDataPacketType::ALPHA_RELATIVE);
    add_type("BETA_RELATIVE", MuseDataPacketType::BETA_RELATIVE);
    add_type("DELTA_RELATIVE", MuseDataPacketType::DELTA_RELATIVE);
    add_type("THETA_RELATIVE", MuseDataPacketType::THETA_RELATIVE);
    add_type("GAMMA_RELATIVE", MuseDataPacketType::GAMMA_RELATIVE);
    add_type("ALPHA_SCORE", MuseDataPacketType::ALPHA_SCORE);
    add_type("BETA_SCORE", MuseDataPacketType::BETA_SCORE);
    add_type("DELTA_SCORE", MuseDataPacketType::DELTA_SCORE);
    add_type("THETA_SCORE", MuseDataPacketType::THETA_SCORE);
    add_type("GAMMA_SCORE", MuseDataPacketType::GAMMA_SCORE);
    add_type("IS_GOOD", MuseDataPacketType::IS_GOOD);
    add_type("HSI", MuseDataPacketType::HSI);
    add_type("HSI_PRECISION", MuseDataPacketType::HSI_PRECISION);
    add_type("ARTIFACTS", MuseDataPacketType::ARTIFACTS);
    combo_data_type_.SetCurSel(0);
    set_accel_ui();
}

void CGettingData32Dlg::add_type(std::string name, MuseDataPacketType type) {
    auto cname = CString(name.c_str());
    combo_data_type_.AddString(cname);
    name_to_type_map_.emplace(cname, type);
}

void CGettingData32Dlg::check_bluetooth_enabled() {
    // This task is async and will update the member variable when it is run.
    create_task([]() -> IVectorView<Radio>
        {
            return Radio::GetRadiosAsync().get();
        }
    ).then([&](IVectorView<Radio> radios)
        {
            for (auto r : radios) {
                if (r.Kind() == RadioKind::Bluetooth) {
                    if (r.State() == RadioState::On) {
                        is_bluetooth_enabled_.store(true);
                    }
                    else {
                        is_bluetooth_enabled_.store(false);
                    }
                    break;
                }
            }
        });
}

bool CGettingData32Dlg::is_bluetooth_enabled() {
    // Must call check_bluetooth_enabled first to run
    // an async check to see if bluetooth radio is on.
    return is_bluetooth_enabled_.load();
}

task<void> CGettingData32Dlg::disconnect_async()
{
    if (my_muse_ == nullptr || my_muse_->get_connection_state() == ConnectionState::DISCONNECTED) {
        return task_from_result();
    }
    my_muse_->disconnect();
    return create_task([&]() {
        while (my_muse_->get_connection_state() != ConnectionState::DISCONNECTED) {
            Sleep(50);
        }
        });
}

// Muse callback methods
void CGettingData32Dlg::muse_list_changed() {
    //OutputDebugString(L"CGettingData32Dlg::muse_list_changed\n");
    ::PostMessage(hwnd, MUSE_LIST_CHANGED, NULL, NULL);
}

void CGettingData32Dlg::receive_log(const LogPacket& log) {
    auto message = log.message;
    if (message.back() != '\n') {
        message += "\n";
    }
    auto msg = new CString(message.c_str());
    ::PostMessage(hwnd, RECEIVE_LOG, NULL, reinterpret_cast<LPARAM>(msg));
}

LRESULT CGettingData32Dlg::OnReceiveLog(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    if (lParam) {
        auto msg = reinterpret_cast<CString*>(lParam);
        OutputDebugString(msg->GetString());
        delete msg;
    }

    return 0;
}

LRESULT CGettingData32Dlg::OnMuseListChanged(WPARAM wParam, LPARAM lParam)
{
    OutputDebugString(L"CGettingData32Dlg::OnMuseListChanged\n");
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    update_muse_list();
    return 0;
}

void CGettingData32Dlg::receive_connection_packet(const MuseConnectionPacket& packet, const std::shared_ptr<Muse>& muse) {
    //OutputDebugString(L"CGettingData32Dlg::receive_connection_packet\n");
    UNREFERENCED_PARAMETER(muse);
    auto args = new MuseConnectionPacket(packet);
    ::PostMessage(hwnd, RECEIVE_CONNECTION_PACKET, NULL, reinterpret_cast<LPARAM>(args));
}

LRESULT CGettingData32Dlg::OnReceiveConnectionPacket(WPARAM wParam, LPARAM lParam)
{
    OutputDebugString(L"CGettingData32Dlg::OnReceiveConnectionPacket\n");
    UNREFERENCED_PARAMETER(wParam);
    if (lParam) {
        auto args = reinterpret_cast<MuseConnectionPacket*>(lParam);
        auto packet = *args;
        model_.set_connection_state(packet.current_connection_state);

        // The Muse version is only available in the connected state.
        if (packet.current_connection_state == ConnectionState::CONNECTED) {
            auto ver = my_muse_->get_muse_version();
            model_.set_version(ver->get_firmware_version());
            my_muse_model_ = my_muse_->get_muse_configuration()->get_model();
        }
        else {
            model_.set_version("Unknown");
        }
        delete args;
    }
    return 0;
}

void CGettingData32Dlg::receive_muse_data_packet(const std::shared_ptr<MuseDataPacket>& packet, const std::shared_ptr<Muse>& muse) {
    //OutputDebugString(L"CGettingData32Dlg::receive_muse_data_packet\n");
    UNREFERENCED_PARAMETER(muse);
    auto args = new MuseDataPacketArgs(packet);
    ::PostMessage(hwnd, RECEIVE_MUSE_DATA_PACKET, NULL, reinterpret_cast<LPARAM>(args));
}

LRESULT CGettingData32Dlg::OnReceiveMuseDataPacket(WPARAM wParam, LPARAM lParam)
{
    //OutputDebugString(L"CGettingData32Dlg::OnReceiveMuseDataPacket\n");
    UNREFERENCED_PARAMETER(wParam);
    if (lParam) {
        auto args = reinterpret_cast<MuseDataPacketArgs*>(lParam);
        auto packet = MuseDataPacket::make_packet(args->packet_type(), args->timestamp(), args->values());
        model_.set_values(packet);
        delete args;
    }
    return 0;
}

void CGettingData32Dlg::receive_muse_artifact_packet(const MuseArtifactPacket& packet, const std::shared_ptr<Muse>& muse) {
    //OutputDebugString(L"CGettingData32Dlg::receive_artifact_packet\n");
    UNREFERENCED_PARAMETER(muse);

    auto args = new MuseArtifactPacket(packet);
    ::PostMessage(hwnd, RECEIVE_MUSE_ARTIFACT_PACKET, NULL, reinterpret_cast<LPARAM>(args));
}

LRESULT CGettingData32Dlg::OnReceiveMuseArtifactPacket(WPARAM wParam, LPARAM lParam)
{
    //OutputDebugString(L"CGettingData32Dlg::OnReceiveMuseArtifactPacket\n");
    UNREFERENCED_PARAMETER(wParam);
    if (lParam) {
        auto args = reinterpret_cast<MuseArtifactPacket*>(lParam);
        model_.set_values(*args);
        delete args;
    }

    return 0;
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CGettingData32Dlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CGettingData32Dlg::OnBnClickedRefresh()
{
    if (!is_bluetooth_enabled_.load()) {
        Uri bt{L"ms-settings:bluetooth"};
        Windows::System::Launcher::LaunchUriAsync(bt);
    }
    manager_->stop_listening();
    manager_->start_listening();
}

void CGettingData32Dlg::OnBnClickedConnect()
{
    CString selected_muse;
    combo_muse_list_.GetLBText(combo_muse_list_.GetCurSel(), selected_muse);
    my_muse_ = get_muse(selected_muse);

    if (nullptr != my_muse_) {
        // Stop listening after selecting a muse to connect to
        manager_->stop_listening();
        model_.clear();
        my_muse_->register_connection_listener(connection_listener_);
        my_muse_->register_data_listener(data_listener_, current_data_type_);
        my_muse_->set_preset(MusePreset::PRESET_21); // Choose a preset to use (see documentation)
        my_muse_->run_asynchronously();
    }
}

void CGettingData32Dlg::OnBnClickedDisconnect()
{
    if (my_muse_ != nullptr) {
        my_muse_->disconnect();
    }
}

void CGettingData32Dlg::OnCbnSelchangeComboDataType()
{
    if (combo_data_type_.GetCurSel() != -1) {
        CString selected;
        combo_data_type_.GetLBText(combo_data_type_.GetCurSel(), selected);
        MuseDataPacketType type = name_to_type_map_.at(selected);

        change_data_type(type);

        switch (type) {
        case MuseDataPacketType::ACCELEROMETER:
            set_accel_ui();
            break;
        case MuseDataPacketType::GYRO:
            set_gyro_ui();
            break;
        case MuseDataPacketType::BATTERY:
            set_battery_ui();
            break;
        case MuseDataPacketType::DRL_REF:
            set_drl_ref_ui();
            break;
        case MuseDataPacketType::ARTIFACTS:
            set_artifacts_ui();
            break;
        case MuseDataPacketType::PPG:
            set_ppg_ui();
            break;
        case MuseDataPacketType::OPTICS:
            set_optics_ui();
            break;
        default:
            // All other packet types derive from EEG data.
            set_eeg_ui();
            break;
        }
    }
    else {
        change_data_type(MuseDataPacketType::EEG);
        set_eeg_ui();
    }
}

void CGettingData32Dlg::queue_ui_update() {
    create_task([]() {
        WaitForSingleObjectEx(GetCurrentThread(), 1000 / REFRESH_RATE, false);
        }).then([]() {
            ::PostMessage(hwnd, UPDATE_UI, NULL, NULL);
            });
}

// Call only from the UI thread.
void CGettingData32Dlg::update_ui() {
    if (model_.is_dirty()) {
        double buffer[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        model_.get_buffer(buffer);

        line_1_data.SetWindowTextW(formatData(buffer[0]));
        line_2_data.SetWindowTextW(formatData(buffer[1]));
        line_3_data.SetWindowTextW(formatData(buffer[2]));
        line_4_data.SetWindowTextW(formatData(buffer[3]));
        line_5_data.SetWindowTextW(formatData(buffer[4]));
        line_6_data.SetWindowTextW(formatData(buffer[5]));
        connection_status.SetWindowTextW(CString(model_.get_connection_state().c_str()));
        version.SetWindowTextW(CString(model_.get_version().c_str()));
        model_.clear_dirty_flag();
    }
    if (!closing_) {
        queue_ui_update();
    }
}

LRESULT CGettingData32Dlg::OnUpdateUI(WPARAM wParam, LPARAM lParam)
{
    //OutputDebugString(L"CGettingData32Dlg::OnUpdateUI\n");
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    update_ui();
    return 0;
}

std::vector<CString> CGettingData32Dlg::get_muse_list() {
    std::vector<CString> list;
    CString item;
    int n;
    for (auto i = 0; i < combo_muse_list_.GetCount(); i++) {
        n = combo_muse_list_.GetLBTextLen(i);
        combo_muse_list_.GetLBText(i, item.GetBuffer(n));
        item.ReleaseBuffer();
        list.push_back(item);
    }
    return list;
}

void CGettingData32Dlg::update_muse_list() {
    auto muses = manager_->get_muses();
    auto old_list = get_muse_list();
    auto cur_sel = combo_muse_list_.GetCurSel();

    combo_muse_list_.ResetContent();
    for (auto& m : muses) {
        auto name = CString(m->get_name().c_str());
        combo_muse_list_.AddString(name);
    }
    if (combo_muse_list_.GetCount()) {
        auto new_list = get_muse_list();
        if (new_list != old_list) {
            auto prev_sel = cur_sel;
            cur_sel = 0;
            if (prev_sel > -1 && prev_sel < old_list.size()) {
                auto prev_item = old_list[prev_sel];
                for (int i = 0; i < new_list.size(); i++) {
                    if (new_list[i] == prev_item) {
                        cur_sel = i;
                        break;
                    }
                }
            }
        }
        combo_muse_list_.SetCurSel(cur_sel);
    }
}

std::shared_ptr<Muse> CGettingData32Dlg::get_muse(CString& desired_name) {
    auto muses = manager_->get_muses();
    for (auto m : muses) {
        auto name = CString(m->get_name().c_str());
        if (name.Compare(desired_name) == 0) {
            return m;
        }
    }
    return nullptr;
}

void CGettingData32Dlg::change_data_type(MuseDataPacketType type) {
    if (my_muse_ != nullptr) {
        my_muse_->unregister_data_listener(data_listener_, current_data_type_);
        my_muse_->register_data_listener(data_listener_, type);
    }
    current_data_type_ = type;
    model_.reset();
}

void CGettingData32Dlg::set_accel_ui() {
    set_ui_line(line_1_label, "X", line_1_data, true);
    set_ui_line(line_2_label, "Y", line_2_data, true);
    set_ui_line(line_3_label, "Z", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_battery_ui() {
    set_ui_line(line_1_label, "CHARGE_PERCENTAGE_REMAINING", line_1_data, true);
    set_ui_line(line_2_label, "MILLIVOLTS", line_2_data, true);
    set_ui_line(line_3_label, "TEMPERATURE_CELSIUS", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_drl_ref_ui() {
    set_ui_line(line_1_label, "DRL", line_1_data, true);
    set_ui_line(line_2_label, "REF", line_2_data, true);
    set_ui_line(line_3_label, "", line_3_data, false);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_eeg_ui() {
    set_ui_line(line_1_label, "EEG1", line_1_data, true);
    set_ui_line(line_2_label, "EEG2", line_2_data, true);
    set_ui_line(line_3_label, "EEG3", line_3_data, true);
    set_ui_line(line_4_label, "EEG4", line_4_data, true);
    auto show_aux = current_data_type_ != MuseDataPacketType::HSI &&
                    current_data_type_ != MuseDataPacketType::HSI_PRECISION;

    if (my_muse_model_ == MuseModel::MS_03) {
        set_ui_line(line_5_label, "AUX1", line_5_data, show_aux);
        set_ui_line(line_6_label, "AUX2", line_6_data, show_aux);
    }
    else {
        set_ui_line(line_5_label, "AUX_LEFT", line_5_data, show_aux);
        set_ui_line(line_6_label, "AUX_RIGHT", line_6_data, show_aux);
    }
}

void CGettingData32Dlg::set_ppg_ui() {
    auto label1 = my_muse_model_ == MuseModel::MU_04 ? "GREEN" :
        my_muse_model_ == MuseModel::MU_05 ? "IR-H16" :
        "AMBIENT";
    set_ui_line(line_1_label, label1, line_1_data, true);
    set_ui_line(line_2_label, "IR", line_2_data, true);
    set_ui_line(line_3_label, "RED", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_gyro_ui() {
    set_ui_line(line_1_label, "X", line_1_data, true);
    set_ui_line(line_2_label, "Y", line_2_data, true);
    set_ui_line(line_3_label, "Z", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_artifacts_ui() {
    set_ui_line(line_1_label, "HEADBAND_ON", line_1_data, true);
    set_ui_line(line_2_label, "BLINK", line_2_data, true);
    set_ui_line(line_3_label, "JAW_CLENCH", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_optics_ui() {
    set_ui_line(line_1_label, "OPTICS1", line_1_data, true);
    set_ui_line(line_2_label, "OPTICS2", line_2_data, true);
    set_ui_line(line_3_label, "OPTICS3", line_3_data, true);
    set_ui_line(line_4_label, "OPTICS4", line_4_data, true);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void CGettingData32Dlg::set_ui_line(CStatic& label, const char* name, CStatic& data, bool visible) {
    label.SetWindowTextW(CString(name));
    data.SetWindowTextW(CString("0.0"));
    int show = visible ? SW_SHOW : SW_HIDE;
    label.ShowWindow(show);
    data.ShowWindow(show);
}

CString CGettingData32Dlg::formatData(double data) const
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(DATA_PRECISION) << data;
    return CString(ss.str().c_str());
}
