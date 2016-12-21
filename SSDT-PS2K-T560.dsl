// tjl - add T560 identifier

DefinitionBlock ("", "SSDT", 2, "PS2K", "T560", 0)
{
    External(\_SB.PCI0, DeviceObj)
    External(\_SB.PCI0.LPC, DeviceObj)

    External(\_SB.PCI0.LPC.PS2K, DeviceObj)
    Scope (\_SB.PCI0.LPC.PS2K)
    {
        // Select specific items in VoodooPS2Controller
        Method(_DSM, 4)
        {
            If (!Arg2) { Return (Buffer() { 0x03 } ) }
            Return (Package()
            {
                "RM,oem-id", "LENOVO",
                "RM,oem-table-id", "T560",
            })
        }
    }
}

//EOF
