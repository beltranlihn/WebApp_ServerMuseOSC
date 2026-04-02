//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

#include <sstream>
#include <iomanip>

#include <ppltasks.h>

#include "muse.h"

using namespace GettingData;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Devices::Radios;

using namespace interaxon;
using namespace concurrency;


// Refresh rate for the data display on the screen (Hz)
#define REFRESH_RATE 60
// Decimal precision for displayed data
#define DATA_PRECISION 4

MainPage::MainPage() :
    current_data_type_(MuseDataPacketType::ACCELEROMETER),
    my_muse_(nullptr),
    enable_data_(true)
{
    InitializeComponent();
    init_data_type_combobox();

    queue_ui_update();

#if _DEBUG
    auto localPath = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
    OutputDebugString(L"Local folder path:\n");
    OutputDebugString(localPath->Data());
    OutputDebugString(L"\n");
#endif

    manager_ = MuseManagerWindows::get_instance();
    muse_listener_ = std::make_shared<GettingData::MuseListenerWin>(this);
    connection_listener_ = std::make_shared<GettingData::ConnectionListener>(this);
    data_listener_ = std::make_shared<GettingData::DataListener>(this);
    manager_->set_muse_listener(muse_listener_);
    is_bluetooth_enabled_.store(false);
    check_bluetooth_enabled();
}

void MainPage::init_data_type_combobox() {
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
    data_type_combobox->SelectedIndex = 0;
}

void MainPage::add_type(Platform::String^ name, MuseDataPacketType type) {
    data_type_combobox->Items->Append(name);
    name_to_type_map_.Insert(name, (int) type);
}

void MainPage::check_bluetooth_enabled() {
    // This task is async and will update the member variable when it is run.
    create_task(Radio::GetRadiosAsync()).then([=](IVectorView<Radio^>^ radios) {
        for (auto r : radios) {
            if (r->Kind == RadioKind::Bluetooth) {
                if (r->State == RadioState::On) {
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

bool MainPage::is_bluetooth_enabled() {
    // Must call check_bluetooth_enabled first to run
    // an async to check if bluetooth radio is on.
    return is_bluetooth_enabled_.load();
}

// Muse callback methods
void MainPage::muse_list_changed() {
    OutputDebugString(L"MainPage::muse_list_changed\n");
    update_muse_list();
}

void MainPage::receive_connection_packet(const MuseConnectionPacket & packet, const std::shared_ptr<Muse> & muse) {
    model_.set_connection_state(packet.current_connection_state);
    OutputDebugString(L"MainPage::receive_connection_packet\n");

    // The Muse version is only available in the connected state.
    if (packet.current_connection_state == ConnectionState::CONNECTED) {
        auto version = my_muse_->get_muse_version();
        model_.set_version(version->get_firmware_version());
        my_muse_model_ = my_muse_->get_muse_configuration()->get_model();
    }
    else {
        model_.set_version("Unknown");
    }

}

void MainPage::receive_muse_data_packet(const std::shared_ptr<MuseDataPacket> & packet, const std::shared_ptr<Muse> & muse) {
    model_.set_values(packet);
    //OutputDebugString(L"MainPage::receive_muse_data_packet\n");
}

void MainPage::receive_muse_artifact_packet(const MuseArtifactPacket & packet, const std::shared_ptr<Muse> & muse) {
    model_.set_values(packet);
    //OutputDebugString(L"MainPage::receive_artifact_packet\n");
}

void MainPage::refresh_button_clicked(Platform::Object^ sender,
    Windows::UI::Xaml::RoutedEventArgs^ e)
{
    manager_->stop_listening();
    manager_->start_listening();
}

void MainPage::connect_button_clicked(Platform::Object^ sender,
    Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto selected_muse = static_cast<Platform::String^>(muse_list_combobox->SelectedItem);
    my_muse_ = get_muse(selected_muse);

    if (nullptr != my_muse_) {
        // Stop listening after selecting a muse to connect to
        manager_->stop_listening();
        model_.clear();
        my_muse_->register_connection_listener(connection_listener_);
        my_muse_->register_data_listener(data_listener_, current_data_type_);
        my_muse_->set_preset(MusePreset::PRESET_21); // Choose a preset that matches with the signals to be received (see documentation)
        my_muse_->run_asynchronously();
    }
}

void MainPage::disconnect_button_clicked(Platform::Object^ sender,
    Windows::UI::Xaml::RoutedEventArgs^ e)
{
    if (my_muse_ != nullptr) {
        my_muse_->disconnect();
    }
}

void GettingData::MainPage::pause_resume_clicked(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e)
{
    if (my_muse_ != nullptr) {
        enable_data_ = !enable_data_;
        my_muse_->enable_data_transmission(enable_data_);
    }
}

void MainPage::data_type_selection_changed(Platform::Object^ sender,
    Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
    if (e->AddedItems->Size > 0) {

        String^ added = (String^)e->AddedItems->GetAt(0);
        MuseDataPacketType type =
            (MuseDataPacketType) name_to_type_map_.Lookup(added);

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

void MainPage::queue_ui_update() {
    create_task([=]() {
        WaitForSingleObjectEx(GetCurrentThread(), 1000 / REFRESH_RATE, false);
    }).then([=]() {
        auto thiz = this;
        Dispatcher->RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            ref new Windows::UI::Core::DispatchedHandler([=]()
            {
                thiz->update_ui();
            })
        );
    });
}

// Call only from the UI thread.
void MainPage::update_ui() {
    if (model_.is_dirty()) {
        double buffer[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        model_.get_buffer(buffer);

        line_1_data->Text = formatData(buffer[0]);
        line_2_data->Text = formatData(buffer[1]);
        line_3_data->Text = formatData(buffer[2]);
        line_4_data->Text = formatData(buffer[3]);
        line_5_data->Text = formatData(buffer[4]);
        line_6_data->Text = formatData(buffer[5]);
        connection_status->Text = Convert::to_platform_string(model_.get_connection_state());
        version->Text = Convert::to_platform_string(model_.get_version());

        model_.clear_dirty_flag();
    }
    queue_ui_update();
}

void MainPage::update_muse_list() {
    auto muses = manager_->get_muses();
    muse_list_combobox->Items->Clear();
    for (auto m : muses) {
        auto name = Convert::to_platform_string(m->get_name());
        muse_list_combobox->Items->Append(name);
    }
    if (muse_list_combobox->Items->Size) {
        muse_list_combobox->SelectedIndex = 0;
    }
}

std::shared_ptr<Muse> MainPage::get_muse(Platform::String^ desired_name) {
    auto muses = manager_->get_muses();
    for (auto m : muses) {
        auto name = Convert::to_platform_string(m->get_name());
        if (name->Equals(desired_name)) {
            return m;
        }
    }
    return nullptr;
}

void MainPage::change_data_type(MuseDataPacketType type) {
    if (my_muse_ != nullptr) {
        my_muse_->unregister_data_listener(data_listener_, current_data_type_);
        my_muse_->register_data_listener(data_listener_, type);
    }
    current_data_type_ = type;
    model_.reset();
}

void MainPage::set_accel_ui() {
    set_ui_line(line_1_label, "X", line_1_data, true);
    set_ui_line(line_2_label, "Y", line_2_data, true);
    set_ui_line(line_3_label, "Z", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_battery_ui() {
    set_ui_line(line_1_label, "CHARGE_PERCENTAGE_REMAINING", line_1_data, true);
    set_ui_line(line_2_label, "MILLIVOLTS", line_2_data, true);
    set_ui_line(line_3_label, "TEMPERATURE_CELSIUS", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_drl_ref_ui() {
    set_ui_line(line_1_label, "DRL", line_1_data, true);
    set_ui_line(line_2_label, "REF", line_2_data, true);
    set_ui_line(line_3_label, "", line_3_data, false);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_eeg_ui() {
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

void MainPage::set_gyro_ui() {
    set_ui_line(line_1_label, "X", line_1_data, true);
    set_ui_line(line_2_label, "Y", line_2_data, true);
    set_ui_line(line_3_label, "Z", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_artifacts_ui() {
    set_ui_line(line_1_label, "HEADBAND_ON", line_1_data, true);
    set_ui_line(line_2_label, "BLINK", line_2_data, true);
    set_ui_line(line_3_label, "JAW_CLENCH", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_ppg_ui() {
    String^ label1 = my_muse_model_ == MuseModel::MU_04 ? "GREEN" :
        (my_muse_model_ == MuseModel::MU_05 ? "IR-H16" : "AMBIENT");
    set_ui_line(line_1_label, label1, line_1_data, true);
    set_ui_line(line_2_label, "IR", line_2_data, true);
    set_ui_line(line_3_label, "RED", line_3_data, true);
    set_ui_line(line_4_label, "", line_4_data, false);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_optics_ui() {
    set_ui_line(line_1_label, "OPTICS1", line_1_data, true);
    set_ui_line(line_2_label, "OPTICS2", line_2_data, true);
    set_ui_line(line_3_label, "OPTICS3", line_3_data, true);
    set_ui_line(line_4_label, "OPTICS4", line_4_data, true);
    set_ui_line(line_5_label, "", line_5_data, false);
    set_ui_line(line_6_label, "", line_6_data, false);
}

void MainPage::set_ui_line(TextBlock^ label, String^ name, TextBlock^ data, bool visible) {
    label->Text = name;
    data->Text = "0.0";

    Windows::UI::Xaml::Visibility visibility = (visible)
        ? Windows::UI::Xaml::Visibility::Visible
        : Windows::UI::Xaml::Visibility::Collapsed;
    label->Visibility = visibility;
    data->Visibility = visibility;
}

Platform::String^ MainPage::formatData(double data) const
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(DATA_PRECISION) << data;
    return Convert::to_platform_string(ss.str());
}

