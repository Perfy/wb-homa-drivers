#include <iostream>

#include "modbus_device.h"

REGISTER_PROTOCOL("modbus", TModbusDevice, TRegisterTypes({
            { TModbusDevice::REG_COIL, "coil", "switch", U8 },
            { TModbusDevice::REG_DISCRETE, "discrete", "switch", U8, true },
            { TModbusDevice::REG_HOLDING, "holding", "text", U16 },
            { TModbusDevice::REG_INPUT, "input", "text", U16, true }
        }));

TModbusDevice::TModbusDevice(PDeviceConfig config, PAbstractSerialPort port)
    : TSerialDevice(config, port), Context(port->LibModbusContext()) {}

uint64_t TModbusDevice::ReadRegister(PRegister reg)
{
    int w = reg->Width();

    if (Port()->Debug())
        std::cerr << "modbus: read " << w << " " << reg->TypeName << "(s) @ " << reg->Address <<
            " of slave " << reg->Slave << std::endl;

    modbus_set_slave(Context->Inner, reg->Slave->Id);
    if (IsSingleBit(reg->Type)) {
        uint8_t b;
        if (w != 1)
            throw TSerialDeviceException(
                "width other than 1 is not currently supported for reg type" +
                reg->TypeName);

        int rc = reg->Type == REG_COIL ?
            modbus_read_bits(Context->Inner, reg->Address, 1, &b) :
            modbus_read_input_bits(Context->Inner, reg->Address, 1, &b);

        if (rc > 0)
            return b;
    } else {
        if (w > 4)
            throw TSerialDeviceException(
                "can't pack more than 4 " + reg->TypeName +
                "s into a single value");

        uint16_t v[4];
        if (reg->Type != REG_HOLDING && reg->Type != REG_INPUT)
            throw TSerialDeviceException("invalid register type");

        int rc = reg->Type == REG_HOLDING ?
            modbus_read_registers(Context->Inner, reg->Address, w, v) :
            modbus_read_input_registers(Context->Inner, reg->Address, w, v);

        if (rc > 0) {
            uint64_t r = 0;
            for (size_t i = 0; i < size_t(w); ++i)
                r = (r << 16) + v[i];
            return r;
        }
    }

    throw TSerialDeviceTransientErrorException(
        "failed to read " + reg->TypeName +
        " @ " + std::to_string(reg->Address));
}

void TModbusDevice::WriteRegister(PRegister reg, uint64_t value)
{
    int w = reg->Width();

    if (Port()->Debug())
        std::cerr << "modbus: write " << w << " " << reg->TypeName << "(s) @ " << reg->Address <<
            " of slave " << reg->Slave << std::endl;

    modbus_set_slave(Context->Inner, reg->Slave->Id);
    switch (reg->Type) {
    case REG_COIL:
        if (w != 1)
            throw TSerialDeviceException(
                "width other than 1 is not currently supported for reg type" +
                reg->TypeName);
        if (modbus_write_bit(Context->Inner, reg->Address, value ? 1 : 0) >= 0)
            return;
        break;
    case REG_HOLDING:
        if (w == 1) {
            if (modbus_write_register(Context->Inner, reg->Address, value) >= 0)
                return;
        } else if (w > 4)
            throw TSerialDeviceException(
                "can't pack more than 4 " + reg->TypeName +
                "s into a single value");
        else {
            uint16_t v[4];
            for (int p = w - 1; p >= 0; --p) {
                v[p] = value & 0xffff;
                value >>= 16;
            }
            // TBD: actually should use modbus_write_and_read_registers
            // and echo back the value that was read (must be configurable)
            if (modbus_write_registers(Context->Inner, reg->Address, w, v) >= 0)
                return;
        }
        break;
    default:
        throw TSerialDeviceException("can't write to " + reg->TypeName);
    }

    throw TSerialDeviceTransientErrorException(
        "failed to write " + reg->TypeName +
        " @ " + std::to_string(reg->Address));
}