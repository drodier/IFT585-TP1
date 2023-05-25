#ifndef _TRANSMISSION_INTERFERENCES_H_
#define _TRANSMISSION_INTERFERENCES_H_

#include <memory>
#include <random>

class Configuration;
class DynamicDataBuffer;

class Interference
{
public:
    static std::unique_ptr<Interference> CreateInterferenceImplementation(const Configuration& config);

    virtual bool noise(DynamicDataBuffer& data) = 0;
};

class NoInterference : public Interference
{
public:
    bool noise(DynamicDataBuffer& data) override;
};

class RandomInterference : public Interference
{
    std::mt19937 m_randomGenerator;
    std::uniform_int_distribution<unsigned int> m_frequencyDistribution;
    std::uniform_int_distribution<unsigned int> m_errorGenerator;
    unsigned int m_frequency;
    unsigned int m_byteErrorFrequency;
public:
    RandomInterference(unsigned int frequency, unsigned int byteErrorFrequency);
    bool noise(DynamicDataBuffer& data) override;
};

class DropFrameInterference : public Interference
{
    std::mt19937 m_randomGenerator;
    std::uniform_int_distribution<unsigned int> m_frequencyDistribution;
    unsigned int m_frequency;
public:
    DropFrameInterference(unsigned int frequency);
    bool noise(DynamicDataBuffer& data) override;
};



#endif //_TRANSMISSION_INTERFERENCES_H_