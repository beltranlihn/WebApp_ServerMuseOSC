#include "pch.h"
#include "DataModel.h"
#include <string>
#include "muse.h"

using namespace interaxon::bridge;

DataModel::DataModel() {

    // EEG packets have the most data of all packet types
    // and EEG packets have a maximum of 6 pieces of data
    // (1 per channel).  Initialize our data buffer to have
    // adequate room to store the data from every packet type.
    data_buffer_ = new double[6];
    clear();
}

DataModel::~DataModel() {
    delete[] data_buffer_;
}

bool DataModel::is_dirty() const {
    return dirty_;
}

void DataModel::clear_dirty_flag() {
    dirty_ = false;
}

void DataModel::clear() {
    dirty_ = false;
    connection_state_string_ = "Unknown";
    version_string_ = "Unknown";
    reset();
}

void DataModel::reset() {
    for (int i = 0; i < 6; i++) {
        data_buffer_[i] = 0.0;
    }
}

// add size_t for length of array
void DataModel::get_buffer(double *buffer) const
{
    for (int i = 0; i < 6; i++) {
        buffer[i] = data_buffer_[i];
    }
}

const std::string &DataModel::get_connection_state() const {
    return connection_state_string_;
}
const std::string &DataModel::get_version() const {
    return version_string_;
}

void DataModel::set_values(const std::shared_ptr<MuseDataPacket> &p) {
    switch (p->packet_type()) {
    case MuseDataPacketType::ACCELEROMETER:
        set_accel_data(p);
        break;
    case MuseDataPacketType::BATTERY:
        set_battery_data(p);
        break;
    case MuseDataPacketType::DRL_REF:
        set_drl_ref_data(p);
        break;
    case MuseDataPacketType::GYRO:
        set_gyro_data(p);
        break;
    case MuseDataPacketType::PPG:
        set_ppg_data(p);
        break;
    case MuseDataPacketType::OPTICS:
        set_optics_data(p);
        break;
    default:
        set_eeg_data(p);
        break;
    }
    dirty_ = true;
}

void DataModel::set_values(const MuseArtifactPacket & p) {
    data_buffer_[0] = p.headband_on ? 1.0 : 0.0;
    data_buffer_[1] = p.blink ? 1.0 : 0.0;;
    data_buffer_[2] = p.jaw_clench ? 1.0 : 0.0;;
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
    dirty_ = true;
}

void DataModel::set_connection_state(ConnectionState state) {
    switch (state) {
    case ConnectionState::DISCONNECTED:
        connection_state_string_ = "DISCONNECTED";
        break;
    case ConnectionState::CONNECTING:
        connection_state_string_ = "CONNECTING";
        break;
    case ConnectionState::CONNECTED:
        connection_state_string_ = "CONNECTED";
        break;
    case ConnectionState::NEEDS_UPDATE:
        connection_state_string_ = "NEEDS_UPDATE";
        break;
    case ConnectionState::NEEDS_LICENSE:
        connection_state_string_ = "NEEDS_LICENSE";
        break;
    case ConnectionState::UNKNOWN:
        connection_state_string_ = "UNKNOWN";
        break;
    }
    dirty_ = true;
}

void DataModel::set_version(const std::string version) {
    version_string_ = version;
    dirty_ = true;
}

void DataModel::set_accel_data(const std::shared_ptr<MuseDataPacket> &p) {
    data_buffer_[0] = p->get_accelerometer_value(Accelerometer::X);
    data_buffer_[1] = p->get_accelerometer_value(Accelerometer::Y);
    data_buffer_[2] = p->get_accelerometer_value(Accelerometer::Z);
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

void DataModel::set_battery_data(const std::shared_ptr<MuseDataPacket> &p) {
    data_buffer_[0] = p->get_battery_value(Battery::CHARGE_PERCENTAGE_REMAINING);
    data_buffer_[1] = p->get_battery_value(Battery::MILLIVOLTS);
    data_buffer_[2] = p->get_battery_value(Battery::TEMPERATURE_CELSIUS);
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

void DataModel::set_drl_ref_data(const std::shared_ptr<MuseDataPacket> &p) {
    data_buffer_[0] = p->get_drl_ref_value(DrlRef::DRL);
    data_buffer_[1] = p->get_drl_ref_value(DrlRef::REF);
    data_buffer_[2] = 0.0;
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

void DataModel::set_eeg_data(const std::shared_ptr<MuseDataPacket> &p) {
    data_buffer_[0] = p->get_eeg_channel_value(Eeg::EEG1);
    data_buffer_[1] = p->get_eeg_channel_value(Eeg::EEG2);
    data_buffer_[2] = p->get_eeg_channel_value(Eeg::EEG3);
    data_buffer_[3] = p->get_eeg_channel_value(Eeg::EEG4);
    data_buffer_[4] = p->get_eeg_channel_value(Eeg::AUX_LEFT);
    data_buffer_[5] = p->get_eeg_channel_value(Eeg::AUX_RIGHT);
}

void DataModel::set_ppg_data(const std::shared_ptr<MuseDataPacket>& p) {
    data_buffer_[0] = p->get_ppg_channel_value(Ppg::AMBIENT);
    data_buffer_[1] = p->get_ppg_channel_value(Ppg::IR);
    data_buffer_[2] = p->get_ppg_channel_value(Ppg::RED);
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

void DataModel::set_optics_data(const std::shared_ptr<MuseDataPacket>& p) {
    data_buffer_[0] = p->get_optics_channel_value(Optics::OPTICS1);
    data_buffer_[1] = p->get_optics_channel_value(Optics::OPTICS2);
    data_buffer_[2] = p->get_optics_channel_value(Optics::OPTICS3);
    data_buffer_[3] = p->get_optics_channel_value(Optics::OPTICS4);
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

void DataModel::set_gyro_data(const std::shared_ptr<MuseDataPacket> &p) {
    data_buffer_[0] = p->get_gyro_value(Gyro::X);
    data_buffer_[1] = p->get_gyro_value(Gyro::Y);
    data_buffer_[2] = p->get_gyro_value(Gyro::Z);
    data_buffer_[3] = 0.0;
    data_buffer_[4] = 0.0;
    data_buffer_[5] = 0.0;
}

