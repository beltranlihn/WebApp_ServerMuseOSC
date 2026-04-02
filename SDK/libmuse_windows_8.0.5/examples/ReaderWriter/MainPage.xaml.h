//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"
#include "muse.h"

using namespace interaxon::bridge;

namespace ReaderWriter
{
    /// <summary>
    /// An example application that demonstrates File I/O operations. 
    /// When the user connects to a headband, the application will stream 
    /// data for 5 seconds writing the EEG data to a file. After 5 seconds, 
    /// the application will close the file and read the file back using a 
    /// ReaderMuse object. The data written is compared against the data 
    /// read to verify that the two sets of data are the same.
    /// </summary>
    public ref class MainPage sealed
    {
    public:
        MainPage();
    private:

        //////////////////////////////////////////////////////
        /// Listener callback functions.
        /// These methods are called by the various listener
        /// class implementations at the end of this file.
        ///

        /// muse_list_changed is called whenever a new headband is detected that
        /// can be connected to or an existing headband is detected as no longer
        /// available.  You must be "listening" in order to receive these callbacks.
        /// To start listening, call MuseManager::start_listening();
        ///
        /// Once you have received the callback, you can get the available
        /// headbands by calling MuseManager::get_muses();
        void muse_list_changed();

        /// You receive a connection packet whenever the state of a headband changes
        /// between DISCONNECTED, CONNECTING and CONNECTED.
        ///
        /// \param packet Contains the prior and new connection states.
        /// \param muse   The muse that sent the connection packet.
        void receive_connection_packet(const MuseConnectionPacket & packet, const std::shared_ptr<Muse> & muse);

        /// Most data from the headband, including raw EEG values, is received as
        /// MuseDataPackets.  While this example only illustrates one packet type
        /// at a time, it is possible to receive multiple data types at the same time.
        /// You can use MuseDataPacket::packet_type();
        ///
        /// \param packet The data packet.
        /// \param muse   The muse that sent the data packet.
        void receive_muse_data_packet(const std::shared_ptr<MuseDataPacket> & packet, const std::shared_ptr<Muse> & muse);

        /// Artifacts are boolean values that are derived from the EEG data
        /// such as eye blinks or clenching of the jaw.
        ///
        /// \param packet The artifact packet.
        /// \param muse   The muse that sent the artifact packet.
        void receive_muse_artifact_packet(const MuseArtifactPacket & packet, const std::shared_ptr<Muse> & muse);

        /// Callback to invoke when reader muse has finished playing back the muse file successfully.
        void receive_playback_done();

        /// Callback to invoke when reader muse encountered any error during playback of muse file.
        void receive_playback_interrupted();


        //////////////////////////////////////////////////////
        ///  UI Callbacks

        /// Called when the user presses the "Refresh" button.
        /// This calls MuseManager::start_listening to start receiving muse_list_changed
        /// callbacks.
        void refresh_button_clicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        /// Called when the user presses the "Connect" button.
        /// This will initiate a connection to the Muse headband selected in the
        /// top drop down.
        void connect_button_clicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        /// Called when the user presses the "Disconnect" button.
        /// This will disconnect from the Muse headband that is currently connected.
        void disconnect_button_clicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);



        //////////////////////////////////////////////////////
        ///  Helper methods

        /// Updates the combobox at the top of the screen with the list of
        /// available Muse headbands.  Called when a muse_list_changed callback
        /// is received.
        void update_muse_list();

        /// Finds the Muse with the specified name in the list of Muses returned
        /// from MuseManager.
        std::shared_ptr<Muse> get_muse(Platform::String^ name);

        /// Update the connection status label
        void update_connection_label(const MuseConnectionPacket & packet);

        /// Update test status label
        void update_status_label(const std::string& status);

        /// Compare the write packets with read packets to make sure they're equal
        void compare_packets();


        //////////////////////////////////////////////////////
        ///  Methods for reading and writing to file.

        /// Method to create a ReaderMuse and start the playback.
        void run_reader_muse();

        /// Method that will disconnect the headband after writing data to
        /// muse file after certain amount of time.
        void run_disconnect_async();


        //////////////////////////////////////////////////////
        ///  Variables

        // A reference to the MuseManager instance.
        std::shared_ptr<interaxon::bridge::MuseManagerWindows> manager_;

        /// The individual listener interfaces in LibMuse are abstract classes.
        /// The following classes are defined at the end of the file.  Each
        /// inner class implements a different interface and forwards the
        /// information received back to this MainPage object.
        friend class ConnectionListener;
        friend class MuseListenerWin;
        friend class DataListener;
        friend class PlaybackListener;

        std::shared_ptr<MuseListenerWin> muse_listener_;
        std::shared_ptr<ConnectionListener> connection_listener_;
        std::shared_ptr<DataListener> data_listener_;
        std::shared_ptr<PlaybackListener> playback_listener_;

        /// MuseFileReader/Writer used for reading and writing files.
        /// ReaderMuse used for playing back the file.
        std::shared_ptr<ReaderMuse> reader_muse_;
        std::shared_ptr<MuseFileWriter> muse_file_writer_;
        std::shared_ptr<MuseFileReader> muse_file_reader_;
        bool is_writing_; // flag to switch b/w reading/writing
        std::vector<std::shared_ptr<MuseDataPacket>> written_data_; // data written to file
        std::vector<std::shared_ptr<MuseDataPacket>> read_back_data_; // data read back from file


        /// A reference to the Muse object that we are currently connected to.
        /// This is useful so we know which Muse to disconnect.
        std::shared_ptr<Muse> my_muse_;

    };


    //////////////////////////////////////////////////////
    ///  Listener Implementation Classes
    ///  These classes implements the various listener 
    ///  abstract classes and forwards calls to methods
    ///  in the MainPage class.

    class MuseListenerWin : public interaxon::bridge::MuseListener {
    public:
        MuseListenerWin(MainPage^ mp) : main_page_(mp) {}
        void muse_list_changed() override {
            main_page_->muse_list_changed();
        }
    private:
        MainPage^ main_page_;
    };

    class ConnectionListener : public interaxon::bridge::MuseConnectionListener {
    public:
        ConnectionListener(MainPage^ mp) : main_page_(mp) {}
        void receive_muse_connection_packet(const MuseConnectionPacket & packet, const std::shared_ptr<Muse> & muse) override {
            main_page_->receive_connection_packet(packet, muse);
        }
    private:
        MainPage^ main_page_;
    };

    class DataListener : public MuseDataListener {
    public:
        DataListener(MainPage^ mp) : main_page_(mp) {}
        void receive_muse_data_packet(const std::shared_ptr<MuseDataPacket> & packet, const std::shared_ptr<Muse> & muse) override {
            main_page_->receive_muse_data_packet(packet, muse);
        }

        void receive_muse_artifact_packet(const MuseArtifactPacket & packet, const std::shared_ptr<Muse> & muse) override {
            main_page_->receive_muse_artifact_packet(packet, muse);
        }
    private:
        MainPage^ main_page_;
    };

    class PlaybackListener : public ReaderPlaybackListener {
    public:
        PlaybackListener(MainPage^ mp) : _mainPage(mp) {}
        void receive_playback_done() override {
            _mainPage->receive_playback_done();
        }

        void receive_playback_interrupted() override {
            _mainPage->receive_playback_interrupted();
        }
    private:
        MainPage^ _mainPage;
    };
}
