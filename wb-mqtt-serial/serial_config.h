#pragma once

#include <string>
#include <memory>
#include <vector>
#include <exception>
#include <map>

#include "register.h"
#include "portsettings.h"
#include "jsoncpp/json/json.h"

struct TDeviceChannel {
    TDeviceChannel(std::string name = "", std::string type = "text",
                   std::string device_id = "", int order = 0,
                   std::string on_value = "", int max = - 1, bool read_only = false,
                   const std::vector<PRegister> regs =
                       std::vector<PRegister>())
        : Name(name), Type(type), DeviceId(device_id),
          Order(order), OnValue(on_value), Max(max),
          ReadOnly(read_only), Registers(regs) {}
    std::string Name;
    std::string Type;
    std::string DeviceId; // FIXME
    int Order;
    std::string OnValue;
    int Max;
    bool ReadOnly;
    std::vector<PRegister> Registers;
};

typedef std::shared_ptr<TDeviceChannel> PDeviceChannel;

struct TDeviceSetupItem {
    TDeviceSetupItem(std::string name, PRegister reg, int value)
        : Name(name), Reg(reg), Value(value) {}
    std::string Name;
    PRegister Reg;
    int Value;
};

typedef std::shared_ptr<TDeviceSetupItem> PDeviceSetupItem;

struct TDeviceConfig {
    static const int DEFAULT_INTER_DEVICE_DELAY_USEC = 100000;
    static const int DEFAULT_ACCESS_LEVEL = 1;

    TDeviceConfig(std::string name = "", int slave_id = 0, std::string protocol = "",
                  int delay_usec = DEFAULT_INTER_DEVICE_DELAY_USEC,
                  int access_level = DEFAULT_ACCESS_LEVEL,
                  int frame_timeout = -1,
                  PRegisterTypeMap type_map = 0)
        : Name(name), SlaveId(slave_id), Protocol(protocol), DelayUSec(delay_usec),
          AccessLevel(access_level), FrameTimeout(frame_timeout), TypeMap(type_map) {}
    int NextOrderValue() const { return DeviceChannels.size() + 1; }
    void AddChannel(PDeviceChannel channel) { DeviceChannels.push_back(channel); };
    void AddSetupItem(PDeviceSetupItem item) { SetupItems.push_back(item); }
    std::string Id;
    std::string Name;
    int SlaveId;
    std::string DeviceType;
    std::string Protocol;
    std::vector<PDeviceChannel> DeviceChannels;
    std::vector<PDeviceSetupItem> SetupItems;
    std::vector<uint8_t> Password;
    int DelayUSec;
    int AccessLevel;
    int FrameTimeout;
    PRegisterTypeMap TypeMap;
};

typedef std::shared_ptr<TDeviceConfig> PDeviceConfig;

struct TPortConfig {
    void AddDeviceConfig(PDeviceConfig device_config) { DeviceConfigs.push_back(device_config); }
    TSerialPortSettings ConnSettings;
    int PollInterval = 20;
    bool Debug = false;
    std::vector<PDeviceConfig> DeviceConfigs;
};

typedef std::shared_ptr<TPortConfig> PPortConfig;

struct THandlerConfig {
    void AddPortConfig(PPortConfig port_config) {
        PortConfigs.push_back(port_config);
        PortConfigs[PortConfigs.size() - 1]->Debug = Debug;
    }
    bool Debug = false;
    std::vector<PPortConfig> PortConfigs;
};

typedef std::shared_ptr<THandlerConfig> PHandlerConfig;

class TConfigParserException: public std::exception {
public:
    TConfigParserException(std::string _message): Message("Error parsing config file: " + _message) {}
    const char* what () const throw () {
        return Message.c_str();
    }
private:
    std::string Message;
};

typedef std::map<std::string, Json::Value> TTemplateMap;

class TConfigTemplateParser {
public:
    TConfigTemplateParser(const std::string& template_config_dir, bool debug);
    TTemplateMap Parse();
    
private:
    void LoadDeviceTemplate(const Json::Value& root, const std::string& filepath);

    std::string DirectoryName;
    bool Debug;
    TTemplateMap Templates;
};

typedef std::function<PRegisterTypeMap(PDeviceConfig device_config)> TGetRegisterTypeMap;

class TConfigParser {
public:
    TConfigParser(const std::string& config_fname, bool force_debug,
                  TGetRegisterTypeMap get_register_type_map,
                  TTemplateMap templates = TTemplateMap());
    PHandlerConfig Parse();
    PRegister LoadRegister(PDeviceConfig device_config, const Json::Value& register_data,
                           std::string& default_type_str);
    void LoadChannel(PDeviceConfig device_config, const Json::Value& channel_data);
    void LoadSetupItem(PDeviceConfig device_config, const Json::Value& item_data);
    void LoadDeviceVectors(PDeviceConfig device_config, const Json::Value& device_data);
    void LoadDevice(PPortConfig port_config, const Json::Value& device_data,
                    const std::string& default_id);
    void LoadPort(const Json::Value& port_data, const std::string& id_prefix);
    void LoadConfig();

private:
    int GetInt(const Json::Value& obj, const std::string& key);
    int ToInt(const Json::Value& v, const std::string& title);

    std::string ConfigFileName;
    PHandlerConfig HandlerConfig;
    TGetRegisterTypeMap GetRegisterTypeMap;
    TTemplateMap Templates;
    Json::Value Root;
};
