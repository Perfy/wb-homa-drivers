#include <iostream>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

// This is the JSON header
#include "jsoncpp/json/json.h"

#include <mosquittopp.h>

#include "sysfs_gpio.h"
#include "common/utils.h"
#include "common/mqtt_wrapper.h"

using namespace std;


struct TGpioDesc
{
    int Gpio;
    bool Inverted;
    string Name;

    TGpioDesc()
        : Inverted(false)
        , Name("")
    {};

};

class THandlerConfig
{
    public:
        vector<TGpioDesc> Gpios;
        void AddGpio(TGpioDesc& gpio_desc) { Gpios.push_back(gpio_desc); };

        string DeviceName;

};

class TMQTTGpioHandler : public TMQTTWrapper
{
    typedef pair<TGpioDesc, TSysfsGpio> TChannelDesc;

	public:
        TMQTTGpioHandler(const TMQTTGpioHandler::TConfig& mqtt_config, const THandlerConfig& handler_config);
		~TMQTTGpioHandler();

		void OnConnect(int rc);
		void OnMessage(const struct mosquitto_message *message);
		void OnSubscribe(int mid, int qos_count, const int *granted_qos);

        void UpdateChannelValues();
        string GetChannelTopic(const TGpioDesc& gpio_desc);

    private:
        THandlerConfig Config;
        vector<TChannelDesc> Channels;

};







TMQTTGpioHandler::TMQTTGpioHandler(const TMQTTGpioHandler::TConfig& mqtt_config, const THandlerConfig& handler_config)
    : TMQTTWrapper(mqtt_config)
    , Config(handler_config)
{
    // init gpios
    for (const TGpioDesc& gpio_desc : handler_config.Gpios) {
        TSysfsGpio gpio_handler(gpio_desc.Gpio, gpio_desc.Inverted);
        gpio_handler.Export();
        if (gpio_handler.IsExported()) {
            gpio_handler.SetOutput();
            Channels.push_back(make_pair(gpio_desc, gpio_handler));
        } else {
            cerr << "ERROR: unable to export gpio " << gpio_desc.Gpio << endl;
        }
    }


	Connect();
};

void TMQTTGpioHandler::OnConnect(int rc)
{
	printf("Connected with code %d.\n", rc);
	if(rc == 0){
		/* Only attempt to Subscribe on a successful connect. */
        string prefix = string("/devices/") + MQTTConfig.Id + "/";

        // Meta
        Publish(NULL, prefix + "/meta/name", Config.DeviceName, 0, true);


        for (TChannelDesc& channel_desc : Channels) {
            auto & gpio_desc = channel_desc.first;

            //~ cout << "GPIO: " << gpio_desc.Name << endl;
            string control_prefix = prefix + "controls/" + gpio_desc.Name;

            Publish(NULL, control_prefix + "/meta/type", "switch", 0, true);

            Subscribe(NULL, control_prefix + "/on");

        }

//~ /devices/293723-demo/controls/Demo-Switch 0
//~ /devices/293723-demo/controls/Demo-Switch/on 1
//~ /devices/293723-demo/controls/Demo-Switch/meta/type switch



	}
}

void TMQTTGpioHandler::OnMessage(const struct mosquitto_message *message)
{
    string topic = message->topic;
    string payload = static_cast<const char *>(message->payload);


    const vector<string>& tokens = split(topic, '/');

    if (  (tokens.size() == 6) &&
          (tokens[0] == "") && (tokens[1] == "devices") &&
          (tokens[2] == MQTTConfig.Id) && (tokens[3] == "controls") &&
          (tokens[5] == "on") )
    {
        for (TChannelDesc& channel_desc : Channels) {
            auto & gpio_desc = channel_desc.first;
            if (tokens[4] == gpio_desc.Name) {
                auto & gpio_handler = channel_desc.second;

                int val = payload == "0" ? 0 : 1;
                if (gpio_handler.SetValue(val) == 0) {
                    // echo, retained
                    Publish(NULL, GetChannelTopic(gpio_desc), payload, 0, true);
                }


            }
        }
    }
}

void TMQTTGpioHandler::OnSubscribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded.\n");
}

string TMQTTGpioHandler::GetChannelTopic(const TGpioDesc& gpio_desc) {
    static string controls_prefix = string("/devices/") + MQTTConfig.Id + "/controls/";
    return (controls_prefix + gpio_desc.Name);
}

void TMQTTGpioHandler::UpdateChannelValues() {
    for (TChannelDesc& channel_desc : Channels) {
        auto & gpio_desc = channel_desc.first;
        auto & gpio_handler = channel_desc.second;

        // vv order matters
        int cached = gpio_handler.GetCachedValue();
        int value = gpio_handler.GetValue();

        if (value >= 0) {
            if ( (cached < 0) || (cached != value)) {
                Publish(NULL, GetChannelTopic(gpio_desc), to_string(value), 0, true); // Publish current value (make retained)
            }
        }
    }

}


int main(int argc, char *argv[])
{
	class TMQTTGpioHandler* mqtt_handler;
	int rc;
    THandlerConfig handler_config;
    TMQTTGpioHandler::TConfig mqtt_config;
    mqtt_config.Host = "localhost";
    mqtt_config.Port = 1883;
    string config_fname;


    int c;
    int digit_optind = 0;
    int aopt = 0, bopt = 0;
    char *copt = 0, *dopt = 0;
    while ( (c = getopt(argc, argv, "c:h:p:")) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 'c':
            printf ("option c with value '%s'\n", optarg);
            config_fname = optarg;
            break;
        case 'p':
            printf ("option p with value '%s'\n", optarg);
            mqtt_config.Port = stoi(optarg);
            break;
        case 'h':
            printf ("option h with value '%s'\n", optarg);
            mqtt_config.Host = optarg;
            break;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }
    //~ if (optind < argc) {
        //~ printf ("non-option ARGV-elements: ");
        //~ while (optind < argc)
            //~ printf ("%s ", argv[optind++]);
        //~ printf ("\n");
    //~ }






    {
        // Let's parse it
        Json::Value root;
        Json::Reader reader;

        if (config_fname.empty()) {
            cerr << "Please specify config file with -c option" << endl;
            return 1;
        }

        ifstream myfile (config_fname);

        bool parsedSuccess = reader.parse(myfile,
                                       root,
                                       false);

        if(not parsedSuccess)
        {
            // Report failures and their locations
            // in the document.
            cerr << "Failed to parse JSON" << endl
               << reader.getFormatedErrorMessages()
               << endl;
            return 1;
        }


        handler_config.DeviceName = root["device_name"].asString();

         // Let's extract the array contained
         // in the root object
        const Json::Value array = root["channels"];

         // Iterate over sequence elements and
         // print its values
        for(unsigned int index=0; index<array.size();
             ++index)
        {
            TGpioDesc gpio_desc;
            gpio_desc.Gpio = array[index]["gpio"].asInt();
            gpio_desc.Name = array[index]["name"].asString();
            //~ gpio_desc.Inverted = array[index]["inverted"].asString();

            handler_config.AddGpio(gpio_desc);

        }
    }



	mosqpp::lib_init();

    mqtt_config.Id = "wb-gpio";
	mqtt_handler = new TMQTTGpioHandler(mqtt_config, handler_config);

	while(1){
		rc = mqtt_handler->loop();
        //~ cout << "break in a loop! " << rc << endl;
		if(rc != 0) {
			mqtt_handler->reconnect();
		} else {
            // update current values

            mqtt_handler->UpdateChannelValues();




        }
	}

	mosqpp::lib_cleanup();

	return 0;
}
//build-dep libmosquittopp-dev libmosquitto-dev
// dep: libjsoncpp0 libmosquittopp libmosquitto


// 2420 2032
// 6008 2348 1972
