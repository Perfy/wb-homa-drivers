#include "fake_modbus.h"

void TFakeModbusContext::USleep(int usec)
{
    Fixture.Emit() << "USleep(" << usec << ")";
}

PFakeSlave TFakeModbusContext::GetSlave(int slave_addr)
{
    auto it = Slaves.find(slave_addr);
    if (it == Slaves.end()) {
        ADD_FAILURE() << "GetSlave(): slave not found, auto-creating: " << slave_addr;
        return AddSlave(slave_addr, PFakeSlave(new TFakeSlave));
    }
    return it->second;
}

PFakeSlave TFakeModbusContext::AddSlave(int slave_addr, PFakeSlave slave)
{
    Fixture.Note() << "AddSlave(" << slave_addr << ")";
    auto it = Slaves.find(slave_addr);
    if (it != Slaves.end()) {
        ADD_FAILURE() << "AddSlave(): slave already present: " << slave_addr;
        return it->second;
    }

    Slaves[slave_addr] = slave;
    return slave;
}

void TFakeModbusContext::Connect()
{
    Fixture.Emit() << "Connect()";
    ASSERT_FALSE(Connected);
    Connected = true;
}

void TFakeModbusContext::Disconnect()
{
    Fixture.Emit() << "Disconnect()";
    ASSERT_TRUE(Connected);
    Connected = false;
}

void TFakeModbusContext::SetDebug(bool debug)
{
    Fixture.Emit() << "SetDebug(" << debug << ")";
    Debug = debug;
}

void TFakeModbusContext::SetSlave(int slave)
{
    Fixture.Emit() << "SetSlave(" << slave << ")";
    CurrentSlave = GetSlave(slave);
}

void TFakeModbusContext::ReadCoils(int addr, int nb, uint8_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_BITS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Coils.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::WriteCoil(int addr, int value)
{
    ASSERT_TRUE(!!CurrentSlave);
    ASSERT_TRUE(value == 0 || value == 1);
    CurrentSlave->Coils[addr] = value;
}

void TFakeModbusContext::ReadDisceteInputs(int addr, int nb, uint8_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_BITS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Discrete.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::ReadHoldingRegisters(int addr, int nb, uint16_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_REGISTERS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Holding.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::WriteHoldingRegisters(int addr, int nb, const uint16_t *data)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_REGISTERS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Holding.WriteRegs(Fixture, addr, nb, data);
}

void TFakeModbusContext::ReadInputRegisters(int addr, int nb, uint16_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_REGISTERS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Input.ReadRegs(Fixture, addr, nb, dest);
}

const char* TFakeModbusConnector::PORT0 = "/dev/ttyNSC0";
const char* TFakeModbusConnector::PORT1 = "/dev/ttyNSC1";

PModbusContext TFakeModbusConnector::CreateContext(const TModbusConnectionSettings& settings)
{
    Fixture.Emit() << "CreateContext(): " << settings;
    if (Contexts.count(settings.Device)) {
        ADD_FAILURE() << "context already created for serial port: " << settings.Device;
        return Contexts[settings.Device];
    }
    PFakeModbusContext context = PFakeModbusContext(new TFakeModbusContext(Fixture));
    Contexts[settings.Device] = context;
    return context;
}

PFakeModbusContext TFakeModbusConnector::GetContext(const std::string& device)
{
    auto it = Contexts.find(device);
    if (it == Contexts.end()) {
        ADD_FAILURE() << "context not found for serial port: " << device;
        TModbusConnectionSettings settings;
        settings.Device = device;
        CreateContext(settings);
    }

    return Contexts[device];
}

PFakeSlave TFakeModbusConnector::GetSlave(const std::string& device, int slave_addr)
{
    return GetContext(device)->GetSlave(slave_addr);
}

PFakeSlave TFakeModbusConnector::AddSlave(const std::string& device, int slave_addr, PFakeSlave slave)
{
    return GetContext(device)->AddSlave(slave_addr, slave);
}

PFakeSlave TFakeModbusConnector::AddSlave(const std::string& device,
                                          int slave_addr,
                                          const TRegisterRange& coil_range,
                                          const TRegisterRange& discrete_range,
                                          const TRegisterRange& holding_range,
                                          const TRegisterRange& input_range)
{
    return AddSlave(device, slave_addr,
                    PFakeSlave(new TFakeSlave(coil_range, discrete_range,
                                              holding_range, input_range)));
}
