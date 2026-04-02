#pragma once

namespace interaxon {

    namespace bridge {
        struct MuseArtifactPacket;
        class MuseDataPacket;
        enum class ConnectionState;
    }
}

/// <summary>
/// DataModel is used to store the information received from the Muse
/// headband until the UI can be updated to reflect it.
/// </summary>
class DataModel {

public:

    DataModel();
    virtual ~DataModel();

    /// Returns true if the model is "dirty", ie. new data has been received
    /// since the last time the UI was updated.
    bool is_dirty() const;

    /// Called by the UI update function to indicate that the UI has been
    /// updated with all of the data contained in the model.
    void clear_dirty_flag();

    /// Erases all data stored in the model.  More heavyweight than clear_dirty_flag
    void clear();

    /// Resets the data buffer to all zeros.
    void reset();

    /// Copies the values of the latest received data packet to the
    /// provided double array.  The double array should be large enough to
    /// store 6 values.
    void get_buffer(double *buffer) const;

    /// Returns the current connection state of the Muse headband.
    const std::string &get_connection_state() const;

    /// Returns the current version of the connected Muse headband.
    /// This value is not available if the headband is not connected.
    const std::string &get_version() const;

    /// Stores the appropriate information from the Muse information packets
    /// in the private variables of this class.
    void set_values(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_values(const interaxon::bridge::MuseArtifactPacket &p);
    void set_connection_state(interaxon::bridge::ConnectionState state);
    void set_version(const std::string &version);

private:

    /// Utility functions for mapping the values in the MuseDataPacket to
    /// the double array data_buffer_
    void set_accel_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_battery_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_drl_ref_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_eeg_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_gyro_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket> &p);
    void set_ppg_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket>& p);
    void set_optics_data(const std::shared_ptr<interaxon::bridge::MuseDataPacket>& p);

    /// Since we are only interested in one data packet type at a time, we
    /// reuse a single double array to store the data and map the data from
    /// the data packet to a structure the UI expects.
    double *data_buffer_;

    /// The String that represents the latest received connection state
    /// information.
    std::string connection_state_string_;

    /// The version of firmware that is running on the headband.
    std::string version_string_;

    /// The dirty state of the model.
    bool dirty_;

};