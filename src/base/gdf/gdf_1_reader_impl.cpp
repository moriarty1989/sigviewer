#include "gdf_1_reader_impl.h"
#include "../stream_utils.h"
#include "../math_utils.h"
#include "../signal_data_block.h"


namespace BioSig_ 
{

GDF1ReaderImpl::GDF1ReaderImpl(QPointer<QFile> file, QPointer<BasicHeader> basic_header)
: file_ (file),
  basic_header_ (basic_header)
{
    // nothing to do here
}

GDF1ReaderImpl::~GDF1ReaderImpl ()
{
    
}


bool GDF1ReaderImpl::loadFixedHeader ()
{
    file_->seek(8);
    basic_header_->setFileSize (file_->size());
    basic_header_->setFullFileName (file_->fileName());
    basic_header_->setType("GDF");
    
    readStreamChars (gdf_header_.gdf_patient_id_, *file_, sizeof(gdf_header_.gdf_patient_id_));
    readStreamChars (gdf_header_.gdf_recording_id_, *file_, sizeof(gdf_header_.gdf_recording_id_));
    readStreamChars (gdf_header_.gdf_start_recording_, *file_, sizeof(gdf_header_.gdf_start_recording_));
    readStreamValue (gdf_header_.gdf_header_size_, *file_);
    readStreamValue (gdf_header_.gdf_equipment_provider_id_, *file_);
    readStreamValue (gdf_header_.gdf_labratory_id_, *file_);
    readStreamValue (gdf_header_.gdf_technican_id_, *file_);
    readStreamValues (gdf_header_.gdf_resered_, *file_, sizeof(gdf_header_.gdf_resered_));
    readStreamValue (gdf_header_.gdf_number_data_records_, *file_);
    readStreamValues (gdf_header_.gdf_duration_data_record_, *file_, sizeof(gdf_header_.gdf_duration_data_record_));
    readStreamValue (gdf_header_.gdf_number_signals_, *file_);

//    recording_time_ = QDateTime::fromString(QString(gdf_start_recording_).left(12),
//                                            "yyyyMMddhhmmss");
    basic_header_->setNumberRecords(gdf_header_.gdf_number_data_records_);
    basic_header_->setRecordDuration(static_cast<float64>(gdf_header_.gdf_duration_data_record_[0]) /
                                     gdf_header_.gdf_duration_data_record_[1]);
//    patient_name_ = gdf_patient_id_;
//    patient_age_ = BasicHeader::UNDEFINED_AGE;
//    patient_sex_ = BasicHeader::UNDEFINED_SEX;
//    doctor_id_ = gdf_technican_id_;
//    hospital_id_ = gdf_labratory_id_;
//    // mistake in specification: 0x2020.. for undefined integer values
//    doctor_id_ =  doctor_id_ == 0x2020202020202020LL ? 0 : doctor_id_;
//    hospital_id_ =  hospital_id_ == 0x2020202020202020LL ? 0 : hospital_id_;
    basic_header_->setRecordsPosition(static_cast<uint32>(gdf_header_.gdf_header_size_));
    basic_header_->setNumberChannels(gdf_header_.gdf_number_signals_);
    
    return true;
}

bool GDF1ReaderImpl::loadSignalHeaders ()
{
    file_->seek(GDF1Header::SIZE);
    gdf_header_.gdf_sig_vector_.resize(gdf_header_.gdf_number_signals_);
    GDF1Header::GDFSignalHeaderVector::iterator sig;
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamChars(sig->label, *file_, sizeof(sig->label));
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamChars(sig->transducer_type, *file_, sizeof (sig->transducer_type));
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig !=gdf_header_. gdf_sig_vector_.end(); sig++)
    {
        readStreamChars(sig->physical_dimension, *file_, sizeof(sig->physical_dimension));
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->physical_minimum, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->physical_maximum, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->digital_minimum, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->digital_maximum, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamChars(sig->pre_filtering, *file_, sizeof(sig->pre_filtering));
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->samples_per_record, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValue(sig->channel_type, *file_);
    }
    for (sig = gdf_header_.gdf_sig_vector_.begin(); sig != gdf_header_.gdf_sig_vector_.end(); sig++)
    {
        readStreamValues(sig->reserved, *file_, sizeof(sig->reserved));
    }
    
    uint32 record_size = 0;
    if (basic_header_->getNumberChannels() > 0)
    {
        uint32 event_sample_rate = 1;
        sig = gdf_header_.gdf_sig_vector_.begin();
        for (uint32 channel_nr = 0;
             sig != gdf_header_.gdf_sig_vector_.end();
             sig++, channel_nr++)
        {
            SignalChannel* channel = new SignalChannel(channel_nr, sig->label,
                                                       sig->samples_per_record,
                                                       sig->physical_dimension,
                                                       sig->physical_minimum,
                                                       sig->physical_maximum,
                                                       sig->digital_minimum,
                                                       sig->digital_maximum,
                                                       sig->channel_type,
                                                       record_size / 8,
                                                       sig->pre_filtering, -1, -1, false);

            basic_header_->addChannel(channel);
            event_sample_rate = lcm(event_sample_rate,
                                     sig->samples_per_record);
            record_size += channel->typeBitSize() *
                            sig->samples_per_record;
            // TODO : signal record-size no multiple of 8 bits
            if (record_size % 8 != 0)
            {
//                if (log_stream_)
//                {
//                    *log_stream_ << "GDFReader::open '" << file_name
//                                 << "' Error: signal record size no multiple of "
//                                 << "8 bits is not supported\n";
//                }
                return false;
            }
        }
        event_sample_rate *= gdf_header_.gdf_duration_data_record_[1];
        
        if (event_sample_rate % gdf_header_.gdf_duration_data_record_[0] != 0)
        {
//            if (log_stream_)
//            {
//                *log_stream_ << "GDFReader::open Warning: event-samplerate is"
//                             << " non integer value\n";
//            }
        }
        event_sample_rate /= gdf_header_.gdf_duration_data_record_[0];
        record_size = (record_size + 7) / 8;
        buffer_ = new int8[record_size];
        basic_header_->setRecordSize(record_size);
        basic_header_->setEventSamplerate (event_sample_rate);
    }    
    return true; 
}

bool GDF1ReaderImpl::loadEventTableHeader ()
{
    return false;
}

void GDF1ReaderImpl::loadSignals (FileSignalReader::SignalDataBlockPtrIterator begin,
                                  FileSignalReader::SignalDataBlockPtrIterator end,
                                 uint32 start_record)
{
    if (!file_->isOpen())
    {
//        if (log_stream_)
//        {
//            *log_stream_ << "GDFReader::loadChannels Error: not open\n";
//        }
        return;
    }

    file_->seek(basic_header_->getRecordsPosition() + start_record * basic_header_->getRecordSize());
    bool something_done = true;
    for (uint32 rec_nr = start_record; something_done; rec_nr++)
    {
        bool rec_out_of_range = (rec_nr >= basic_header_->getNumberRecords());
        if (!rec_out_of_range)
        {
            readStreamValues(buffer_, *file_, basic_header_->getRecordSize());
        }
        something_done = false;
        for (FileSignalReader::SignalDataBlockPtrIterator data_block = begin;
             data_block != end;
             data_block++)
        {
            if ((*data_block)->sub_sampling > 1 ||
                (*data_block)->channel_number >= basic_header_->getNumberChannels())
            {
//                if (log_stream_)
//                {
//                    *log_stream_ << "GDFReader::loadChannels Error: "
//                                 << "invalid SignalDataBlock\n";
//                }
                continue;
            }
            SignalChannel* sig = basic_header_->getChannelPointer((*data_block)->channel_number);
            uint32 samples = sig->getSamplesPerRecord();
            uint32 actual_sample = (rec_nr - start_record) * samples;
            if (actual_sample + samples > (*data_block)->number_samples)
            {
                if (actual_sample >= (*data_block)->number_samples)
                {
                    (*data_block)->setBufferOffset(start_record * samples);
                    continue;   // signal data block full
                }
                samples = (*data_block)->number_samples - actual_sample;
            }
            float32* data_block_buffer = (*data_block)->getBuffer();
            float32* data_block_upper_buffer = (*data_block)->getUpperBuffer();
            float32* data_block_lower_buffer = (*data_block)->getLowerBuffer();
            bool* data_block_buffer_valid = (*data_block)->getBufferValid();
            something_done = true;
            if (rec_out_of_range)
            {
                for (uint32 samp = actual_sample;
                     samp < actual_sample + samples;
                     samp++)
                {
                    data_block_buffer_valid[samp] = false;
                }
            }
            else
            {
                convertData(buffer_, &data_block_buffer[actual_sample],
                            *sig, samples);
                for (uint32 samp = actual_sample;
                     samp < actual_sample + samples;
                     samp++)
                {
                    data_block_upper_buffer[samp] = data_block_buffer[samp];
                    data_block_lower_buffer[samp] = data_block_buffer[samp];
                    data_block_buffer_valid[samp] 
                        = data_block_buffer[samp] > sig->getPhysicalMinimum() &&
                          data_block_buffer[samp] < sig->getPhysicalMaximum();
                }
            }
        }
    }
}


}