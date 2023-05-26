#include "LinkLayerLow.h"

#include "LinkLayer.h"
#include "../NetworkDriver.h"
#include "../../../DataStructures/DataBuffer.h"
#include "../../../General/Configuration.h"
#include "../../../General/Logger.h"

#include <iostream>

std::unique_ptr<DataEncoderDecoder> DataEncoderDecoder::CreateEncoderDecoder(const Configuration& config)
{
    int encoderDecoderConfig = config.get(Configuration::LINK_LAYER_LOW_DATA_ENCODER_DECODER);
    if (encoderDecoderConfig == 1)
    {
        return std::make_unique<HammingDataEncoderDecoder>();
    }
    else if (encoderDecoderConfig == 2)
    {
        return std::make_unique<CRCDataEncoderDecoder>();
    }
    else
    {
        return std::make_unique<PassthroughDataEncoderDecoder>();
    }
}


DynamicDataBuffer PassthroughDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    return data;
}

std::pair<bool, DynamicDataBuffer> PassthroughDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    return std::pair<bool, DynamicDataBuffer>(true, data);
}


//===================================================================
// Hamming Encoder decoder implementation
//===================================================================
HammingDataEncoderDecoder::HammingDataEncoderDecoder()
{
    // À faire TP1 (si Hamming demandé)
}

HammingDataEncoderDecoder::~HammingDataEncoderDecoder()
{
    // À faire TP1 (si Hamming demandé)
}

DynamicDataBuffer HammingDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    // À faire TP1 (si Hamming demandé)
    return data;
}

std::pair<bool, DynamicDataBuffer> HammingDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    // À faire TP1 (si Hamming demandé)
    return std::pair<bool, DynamicDataBuffer>(true, data);
}



//===================================================================
// CRC Encoder decoder implementation
//===================================================================
CRCDataEncoderDecoder::CRCDataEncoderDecoder()
{
    generator = 0b100000001;
}

CRCDataEncoderDecoder::~CRCDataEncoderDecoder()
{
    // À faire TP1 (si CRC demandé)
}

DynamicDataBuffer CRCDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    DynamicDataBuffer* encoded_data = new DynamicDataBuffer(data.size() + 1);

    // On vient ajouter le padding sur les bits de poids faible de la trame
    for (int i = 0; i < encoded_data->size(); i++)
    {
        if (i == encoded_data->size() - 1)
            encoded_data->operator[](i) = 0x00;
        else
            encoded_data->operator[](i) = data[i];
    }

    uint8_t reste = 0;

    // Ici on fait la division binaire pour aller caculer le code CRC
    for (int i = 0; i < encoded_data->size(); i++)
    {
        reste ^= encoded_data->operator[](i);

        for (int j = 0; j < 8; j++)
        {
            if (reste & 0x80)
                reste = (reste << 1) ^ generator;
            else
                reste <<= 1;
        }
    }

    // On ajoute le code CRC dans le padding qui avait été ajouté au début
    encoded_data->operator[](encoded_data->size() - 1) = reste;

    return *encoded_data;
}

std::pair<bool, DynamicDataBuffer> CRCDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    uint8_t reste = 0;

    // Ici on fait la division binaire pour vérifier qu'il ne reste rien suite à la division
    for (int i = 0; i < data.size(); i++)
    {
        reste ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (reste & 0x80)
                reste = (reste << 1) ^ generator;
            else
                reste <<= 1;
        }
    }

    // On recré la trame en retirant le padding
    DynamicDataBuffer* decoded_data = new DynamicDataBuffer(data.size() - 1);

    // On recopie le contenu de la trame dans les donnée décodé
    for (int i = 0; i < decoded_data->size(); i++)
    {
        decoded_data->operator[](i) = data[i];
    }

    return std::pair<bool, DynamicDataBuffer>(reste == 0, *decoded_data);
}


//===================================================================
// Network Driver Physical layer implementation
//===================================================================
LinkLayerLow::LinkLayerLow(NetworkDriver* driver, const Configuration& config)
    : m_driver(driver)
    , m_sendingBuffer(config.get(Configuration::LINK_LAYER_LOW_SENDING_BUFFER_SIZE))
    , m_receivingBuffer(config.get(Configuration::LINK_LAYER_LOW_RECEIVING_BUFFER_SIZE))
    , m_stopReceiving(true)
    , m_stopSending(true)
{
    m_encoderDecoder = DataEncoderDecoder::CreateEncoderDecoder(config);
}

LinkLayerLow::~LinkLayerLow()
{
    stop();
    m_driver = nullptr;
}

void LinkLayerLow::start()
{
    stop();

    start_receiving();
    start_sending();
}

void LinkLayerLow::stop()
{
    stop_receiving();
    stop_sending();
}

bool LinkLayerLow::dataReceived() const
{
    return m_receivingBuffer.canRead<DynamicDataBuffer>();
}

DynamicDataBuffer LinkLayerLow::encode(const DynamicDataBuffer& data) const
{
    return m_encoderDecoder->encode(data);
}

std::pair<bool, DynamicDataBuffer> LinkLayerLow::decode(const DynamicDataBuffer& data) const
{
    return m_encoderDecoder->decode(data);
}

void LinkLayerLow::start_receiving()
{
    m_stopReceiving = false;
    m_receivingThread = std::thread(&LinkLayerLow::receiving, this);
}

void LinkLayerLow::stop_receiving()
{
    m_stopReceiving = true;
    if (m_receivingThread.joinable())
    {
        m_receivingThread.join();
    }
}

void LinkLayerLow::start_sending()
{
    m_stopSending = false;
    m_sendingThread = std::thread(&LinkLayerLow::sending, this);
}

void LinkLayerLow::stop_sending()
{
    m_stopSending = true;
    if (m_sendingThread.joinable())
    {
        m_sendingThread.join();
    }
}


void LinkLayerLow::receiving()
{
    while (!m_stopReceiving)
    {
        if (dataReceived())
        {
            DynamicDataBuffer data = m_receivingBuffer.pop<DynamicDataBuffer>();
            std::pair<bool, DynamicDataBuffer> dataBuffer = decode(data);
            if (dataBuffer.first) // Les donnees recues sont correctes et peuvent etre utilisees
            {
                Frame frame = Buffering::unpack<Frame>(dataBuffer.second);
                m_driver->getLinkLayer().receiveData(frame);
            }
            else
            {
                // Les donnees recues sont corrompues et doivent etre delaissees
                Logger log(std::cout);
                log << m_driver->getMACAddress() << " : Corrupted data received" << std::endl;
            }
        }
    }
}

void LinkLayerLow::sending()
{
    while (!m_stopSending)
    {
        if (m_driver->getLinkLayer().dataReady())
        {
            Frame dataFrame = m_driver->getLinkLayer().getNextData();
            DynamicDataBuffer buffer = encode(Buffering::pack<Frame>(dataFrame));
            sendData(buffer);
        }
    }
}

void LinkLayerLow::receiveData(const DynamicDataBuffer& data)
{
    // Si le buffer est plein, on fait juste oublier les octets recus du cable
    // Sinon, on ajoute les octets au buffer
    if (m_receivingBuffer.canWrite<DynamicDataBuffer>(data))
    {
        m_receivingBuffer.push(data);
    }
    else
    {
        Logger log(std::cout);
        log << m_driver->getMACAddress() << " : Physical reception buffer full... data discarded" << std::endl;
    }
}

void LinkLayerLow::sendData(DynamicDataBuffer data)
{
    // Envoit une suite d'octet sur le cable connecte
    m_driver->sendToCard(data);
}