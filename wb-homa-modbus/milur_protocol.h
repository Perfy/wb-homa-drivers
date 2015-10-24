#pragma once

#include <string>
#include <memory>
#include <exception>
#include <unordered_map>
#include <stdint.h>

#include "em_protocol.h"
#include "regformat.h"

class TMilurProtocol: public TEMProtocol {
public:
    static const int DefaultTimeoutMs = 1000;

    TMilurProtocol(PAbstractSerialPort port);
    uint64_t ReadRegister(uint8_t slave, uint8_t address, RegisterFormat fmt);

protected:
    bool ConnectionSetup(uint8_t slave);

private:
    const uint8_t ACCESS_LEVEL = 1;
};

typedef std::shared_ptr<TMilurProtocol> PMilurProtocol;
