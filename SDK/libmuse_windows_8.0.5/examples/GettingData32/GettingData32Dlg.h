
// GettingData32Dlg.h : header file
//

#pragma once
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <ppltasks.h>

#include <map>
#include <memory>
#include <iomanip>
#include "muse.h"
#include "DataModel.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.UI.Core.h>

using namespace winrt;
using namespace interaxon;
using namespace interaxon::bridge;
using namespace Windows::Devices;
using namespace Windows::Devices::Radios;
using namespace Windows::Foundation;
using namespace Concurrency;

// CGettingData32Dlg dialog
class CGettingData32Dlg : public CDialogEx
{
    // Construction
public:
    CGettingData32Dlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_GETTINGDATA32_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
// Implementation
protected:
    HICON m_hIcon;
    bool closing_;

    // Generated message map functions
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()

    CComboBox combo_muse_list_;
    CComboBox combo_data_type_;
    CStatic line_1_label, line_1_data;
    CStatic line_2_label, line_2_data;
    CStatic line_3_label, line_3_data;
    CStatic line_4_label, line_4_data;
    CStatic line_5_label, line_5_data;
    CStatic line_6_label, line_6_data;
    CStatic connection_status;
    CStatic version;

    //////////////////////////////////////////////////////
    /// Initialization functions

    /// Initializes the combobox for selecting which type of
    /// data you want to receive.
    void init_data_type_combobox();

    /// Adds a name to the combobox for selecting which type of
    /// data you want to receive and associates a packet type with
    /// the name.
    void add_type(std::string name, MuseDataPacketType type);

    //////////////////////////////////////////////////////
    /// Bluetooth functions

    /// Creates an asynchronous task to check if there are
    /// any Bluetooth radios that are currently turned on.
    /// The result is accessible from is_bluetooth_enabled.
    void check_bluetooth_enabled();

    /// Returns the last result from the call to check_bluetooth_enabled().
    /// Returns true if a Bluetooth radio is on, false if all Bluetooth
    /// radios are off.
    bool is_bluetooth_enabled();

    task<void> disconnect_async();

    void CloseAsync();

    //////////////////////////////////////////////////////
    /// Listener callback functions.
    ///

    /// muse_list_changed is called whenever a new headband is detected that
    /// can be connected to or an existing headband is detected as no longer
    /// available.  You must be "listening" in order to receive these callbacks.
    /// To start listening, call MuseManager::start_listening();
    ///
    /// Once you have received the callback, you can get the available
    /// headbands by calling MuseManager::get_muses();
    void muse_list_changed();
    std::vector<CString> get_muse_list();

    void receive_log(const LogPacket& log);

    /// You receive a connection packet whenever the state of a headband changes
    /// between DISCONNECTED, CONNECTING and CONNECTED.
    ///
    /// \param packet Contains the prior and new connection states.
    /// \param muse   The muse that sent the connection packet.
    void receive_connection_packet(const MuseConnectionPacket& packet, const std::shared_ptr<Muse>& muse);

    /// Most data from the headband, including raw EEG values, is received as
    /// MuseDataPackets.  While this example only illustrates one packet type
    /// at a time, it is possible to receive multiple data types at the same time.
    /// You can use MuseDataPacket::packet_type();
    ///
    /// \param packet The data packet.
    /// \param muse   The muse that sent the data packet.
    void receive_muse_data_packet(const std::shared_ptr<MuseDataPacket>& packet, const std::shared_ptr<Muse>& muse);

    /// Artifacts are boolean values that are derived from the EEG data
    /// such as eye blinks or clenching of the jaw.
    ///
    /// \param packet The artifact packet.
    /// \param muse   The muse that sent the artifact packet.
    void receive_muse_artifact_packet(const MuseArtifactPacket& packet, const std::shared_ptr<Muse>& muse);



    //////////////////////////////////////////////////////
    ///  Helper functions for the UI

    /// EEG Data is received at a much higher rate (220 or 256Hz) than it is
    /// necessary to update the UI.  This function will ask for a UI update
    /// at a more reasonable rate (60Hz).
    void queue_ui_update();

    /// Handles a single update of the screen by transferring the data from
    /// the DataModel to the UI elements.
    ///
    /// Called from queue_ui_update on the Core Dispatcher.
    void update_ui();


    /// Updates the combobox at the top of the screen with the list of
    /// available Muse headbands.  Called when a muse_list_changed callback
    /// is received.
    void update_muse_list();

    /// Finds the Muse with the specified name in the list of Muses returned
    /// from MuseManager.
    std::shared_ptr<Muse> get_muse(CString& name);

    /// Unregisters the current MuseDataListener and registers a listener for
    /// the new data type.
    ///
    /// \param type The MuseDataPacketType you want to display.
    void change_data_type(MuseDataPacketType type);

    /// Update the titles of the displayed data to reflect the type of packet
    /// that we are receiving; Accelerometer, Battery, DrlRef, EEG, Gyro or
    /// Artifact respectively.  EEG is used for all EEG derived values like
    /// ALPHA_ABSOLUTE, BETA_RELATIVE, HSI_PRECISION, etc.
    void set_accel_ui();
    void set_battery_ui();
    void set_drl_ref_ui();
    void set_eeg_ui();
    void set_ppg_ui();
    void set_gyro_ui();
    void set_artifacts_ui();
    void set_optics_ui();

    /// Sets the text on a single line to the specified value.  Hides or
    /// shows the line as appropriate.
    ///
    /// \param label The TextBlock that defines the type of data.
    /// \param name  The value to set as the Text in "label".
    /// \param data  The TextBlock that defines the value of the data.
    ///              This will be initialized to 0.0
    /// \param visible True if the line should be shown, false if it should
    ///                be hidden.
    void set_ui_line(CStatic& label, const char* name, CStatic& data, bool visible);

    /// Formats a double value to a String with the desired number of
    /// decimal places.
    CString formatData(double data) const;


    //////////////////////////////////////////////////////
    ///  Variables

    // A reference to the MuseManager instance.
    std::shared_ptr<interaxon::bridge::MuseManagerWindows> manager_;

    /// The individual listener interfaces in LibMuse are abstract classes.
    /// The following classes are defined at the end of the file.  Each
    /// inner class implements a different interface and forwards the
    /// information received back to this MainPage object.
   // friend class ConnectionListener;
    //friend class MuseListenerWin;
    //friend class DataListener;

    std::shared_ptr<interaxon::bridge::MuseListener> muse_listener_;
    std::shared_ptr<interaxon::bridge::MuseConnectionListener> connection_listener_;
    std::shared_ptr<interaxon::bridge::MuseDataListener> data_listener_;
    std::shared_ptr<interaxon::bridge::LogListener> log_listener_;

    /// A reference to the Muse object that we are currently connected to.
    /// This is useful so we know which Muse to disconnect.
    std::shared_ptr<Muse> my_muse_;
    MuseModel my_muse_model_;

    /// The current type of data that we are listening to.  This is set
    /// from the combobox in the middle of the screen.
    MuseDataPacketType current_data_type_;

    /// The Data Model object that is used to collect and store the data
    /// we receive.
    DataModel model_;

    /// The last result returned from the check_bluetooth_enabled function.
    std::atomic_bool is_bluetooth_enabled_;

    /// A map for getting the MuseDataPacketType from the name in the
    /// data type combobox.
    std::map<CString, MuseDataPacketType> name_to_type_map_;

public:
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnBnClickedConnect();
    afx_msg void OnBnClickedDisconnect();
    afx_msg void OnCbnSelchangeComboDataType();
    afx_msg LRESULT OnMuseListChanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnReceiveConnectionPacket(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnReceiveMuseArtifactPacket(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnReceiveMuseDataPacket(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdateUI(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnReceiveLog(WPARAM wParam, LPARAM lParam);

    //////////////////////////////////////////////////////
    ///  Listener Implementation Classes

    class LogListenerWin : public interaxon::bridge::LogListener
    {
    public:
        LogListenerWin(CGettingData32Dlg mp) : main_page_(mp) {}
        void receive_log(const LogPacket& log) override {
            main_page_.receive_log(log);
        }
    private:
        CGettingData32Dlg& main_page_;

    };

    class MuseListenerWin : public interaxon::bridge::MuseListener {
    public:
        MuseListenerWin(CGettingData32Dlg mp) : main_page_(mp) {}
        void muse_list_changed() override {
            main_page_.muse_list_changed();
        }
    private:
        CGettingData32Dlg& main_page_;
    };

    class ConnectionListener : public interaxon::bridge::MuseConnectionListener {
    public:
        ConnectionListener(CGettingData32Dlg mp) : main_page_(mp) {}
        void receive_muse_connection_packet(const MuseConnectionPacket& packet, const std::shared_ptr<Muse>& muse) override {
            main_page_.receive_connection_packet(packet, muse);
        }
    private:
        CGettingData32Dlg& main_page_;
    };

    class DataListener : public MuseDataListener {
    public:
        DataListener(CGettingData32Dlg mp) : main_page_(mp) {}
        void receive_muse_data_packet(const std::shared_ptr<MuseDataPacket>& packet, const std::shared_ptr<Muse>& muse) override {
            main_page_.receive_muse_data_packet(packet, muse);
        }

        void receive_muse_artifact_packet(const MuseArtifactPacket& packet, const std::shared_ptr<Muse>& muse) override {
            main_page_.receive_muse_artifact_packet(packet, muse);
        }
    private:
        CGettingData32Dlg& main_page_;
    };

    class MuseDataPacketArgs
    {
    public:
        MuseDataPacketArgs(const std::shared_ptr<MuseDataPacket>& packet) : 
            type_(packet->packet_type()),
            timestamp_(packet->timestamp()),
            values_(packet->values())
        {    
        }
        MuseDataPacketType packet_type()
        {
            return type_;
        }
        int64_t timestamp()
        {
            return timestamp_;
        }
        std::vector<double> values()
        {
            return values_;
        }
    private:
        MuseDataPacketType type_;
        int64_t timestamp_;
        std::vector<double> values_;
    };
};

