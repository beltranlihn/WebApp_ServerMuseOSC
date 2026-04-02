//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace ReaderWriter;

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
using namespace Concurrency;

MainPage::MainPage() :
    my_muse_(nullptr)
{
    InitializeComponent();

    manager_ = MuseManagerWindows::get_instance();
    muse_listener_ = std::make_shared<MuseListenerWin>(this);
    connection_listener_ = std::make_shared<ConnectionListener>(this);
    data_listener_ = std::make_shared<DataListener>(this);
    playback_listener_ = std::make_shared<PlaybackListener>(this);
    manager_->set_muse_listener(muse_listener_);

    // Must create muse file writer in a separate thread to not block UI, otherwise app will crash.
    create_task([this] {
        // Create a file in the applications local folder. Must pass in absolute path of file name.
        auto path = Convert::to_std_string(Windows::Storage::ApplicationData::Current->LocalFolder->Path);
        muse_file_writer_ = MuseFileFactory::get_muse_file_writer(path + "\\ReaderWriterFile");
        is_writing_ = true;
    });
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
        my_muse_->register_connection_listener(connection_listener_);
        my_muse_->register_data_listener(data_listener_, MuseDataPacketType::EEG);
        my_muse_->run_asynchronously();
        update_status_label("Starting...");
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

void MainPage::disconnect_button_clicked(Platform::Object^ sender,
    Windows::UI::Xaml::RoutedEventArgs^ e)
{
    if (my_muse_ != nullptr) {
        my_muse_->disconnect();
    }
}

void MainPage::muse_list_changed() {
    OutputDebugString(L"MainPage::muse_list_changed\n");
    update_muse_list();
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

void MainPage::receive_connection_packet(const MuseConnectionPacket & packet, const std::shared_ptr<Muse> & muse) {
    OutputDebugString(L"MainPage::receive_connection_packet\n");
    update_connection_label(packet);

    // The Muse version is only available in the connected state.
    if (packet.current_connection_state == ConnectionState::CONNECTED) {
        written_data_.clear(); // reset data in subsequent runs
        read_back_data_.clear();
        muse_file_writer_->open(); // Open the file for writing.
        is_writing_ = true; // Set flag to true, so receive_data_packet will write data to file.
        run_disconnect_async(); // Automatically disconnect after writing data for some time.
        update_status_label("Writing data..."); // Update UI label.
    }
    else if (packet.current_connection_state == ConnectionState::DISCONNECTED){
        muse_file_writer_->flush(); // This will actually write the data to file.
        muse_file_writer_->close(); // Must close file before opening it for reading.
        run_reader_muse();
    }

}

void MainPage::receive_muse_data_packet(const std::shared_ptr<MuseDataPacket> & packet, const std::shared_ptr<Muse> & muse) {
    if (is_writing_) {
        // First param is just an id for the muse.
        written_data_.push_back(packet);
        muse_file_writer_->add_data_packet(1, packet);
    } else {
        read_back_data_.push_back(packet);
    }
}

void MainPage::receive_muse_artifact_packet(const MuseArtifactPacket & packet, const std::shared_ptr<Muse> & muse) {
    OutputDebugString(L"MainPage::receive_artifact_packet\n");
}

void ReaderWriter::MainPage::run_disconnect_async()
{
    create_task([this]() {
        // Sleep for 5 seconds to allow writing of data to file for 5 sec.
        WaitForSingleObjectEx(GetCurrentThread(), 5000, false);
    }).then([this]() {
        OutputDebugString(L"MainPage::run_disconnect_async running\n");
        my_muse_->disconnect();
    });
}

void ReaderWriter::MainPage::run_reader_muse()
{
    create_task([this] {
        OutputDebugString(L"MainPage::run_reader_muse\n");
        // Create MuseFileReader
        auto path = Convert::to_std_string(Windows::Storage::ApplicationData::Current->LocalFolder->Path);
        muse_file_reader_ = MuseFileFactory::get_muse_file_reader(path + "\\ReaderWriterFile");

        // Create eventloop used to simulate playback with saved timestamp.
        // This is not needed if playback setting is AS_FAST_AS_POSSIBLE.
        auto event_loop = EventLoopFactory::get_event_loop();

        // Create ReaderMuse with the right model and playback setting.
        auto rmb = ReaderMuseBuilder::get();
        auto model = my_muse_->is_low_energy() ? MuseModel::MU_02 : MuseModel::MU_01;
        reader_muse_ = rmb->with_model(model)->with_event_loop(event_loop)->build(muse_file_reader_);
        reader_muse_->set_playback_settings(ReaderMusePlaybackSettings::SIMULATED_WITH_SAVED_TIMESTAMP);

        // Set the playback listener and register for data listener.
        reader_muse_->as_muse()->register_data_listener(data_listener_, MuseDataPacketType::EEG);
        reader_muse_->set_playback_listener(playback_listener_);

        // Toggle write flag and start the playback.
        is_writing_ = false;
        reader_muse_->playback();
        update_status_label("Reading back data...");
    });
}

void MainPage::receive_playback_interrupted()
{
    OutputDebugString(L"MainPage::receive_playback_interrupted\n");
    update_status_label("Failed - playback interrupted");
}

void MainPage::receive_playback_done()
{
    create_task([this] {
        muse_file_reader_->close();
        auto write_size = written_data_.size();
        auto read_size = read_back_data_.size();

        if (write_size != read_size) {
            // Update test status to FAILED
            update_status_label("Failed - size of read data not same as write data");
        } else {
            // Check each packet has the same value
            compare_packets();
        }
    });
}

void MainPage::compare_packets() {
    uint32_t good_packets = 0;
    uint32_t bad_packets = 0;
    auto write_size = written_data_.size();

    for (int i = 0; i < written_data_.size(); i++) {
        auto w_data = written_data_[i]->values();
        auto r_data = read_back_data_[i]->values();

        // Default preset has only 4 channel of values
        for (int j = 0; j < 4; j++) {
            // Comparison of two double value is consider equal if their
            // diference is less than the epsilon (0.001 in this case).
            if (std::abs(w_data[j] - r_data[j]) > 0.001) {
                bad_packets++;
                break;
            }
            else {
                good_packets++;
            }
        }
    }

    auto total_packets = good_packets + bad_packets;
    if (bad_packets > 0) {
        auto description = "Failed - " + std::to_string(bad_packets) + "/" + std::to_string(total_packets) + " packets are not equal";
        update_status_label(description);
    }
    else if (good_packets == 0)
    {
        update_status_label("Failed - no packets were written");
    }
    else {
        update_status_label("Passed - all read and write packets are equal");
    }
}

void ReaderWriter::MainPage::update_connection_label(const MuseConnectionPacket & packet)
{
    // Update the connection status UI text
    Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
        ref new Windows::UI::Core::DispatchedHandler([this, packet]()
        {
            Platform::String^ state = "";

            switch (packet.current_connection_state)
            {
            case ConnectionState::CONNECTED:
                state = "Connected";
                break;
            case ConnectionState::CONNECTING:
                state = "Connecting";
                break;
            case ConnectionState::DISCONNECTED:
                state = "Disconnected";
                break;
            case ConnectionState::NEEDS_LICENSE:
                state = "Unlicensed";
                break;
            default:
                state = "Unknown";
                break;
            }
            connection_status->Text = state;
        })
    );
}

void ReaderWriter::MainPage::update_status_label(const std::string & status)
{
    // Update the test status UI text
    Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
        ref new Windows::UI::Core::DispatchedHandler([this, status]()
    {
        auto text = Convert::to_platform_string(status);
        test_status->Text = text;
    }));
}

