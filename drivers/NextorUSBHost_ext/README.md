# NextorUSBHost_ext

This is an alternate MSXUSB firmware image for setups where another cartridge,
such as an SDMapper, is the active Nextor boot cartridge.

The normal `NextorUsbHost` firmware builds a complete bootable Nextor ROM. When
that complete ROM is inserted in a secondary slot, the booting Nextor kernel from
another cartridge sees the MSXUSB cartridge as another Nextor ROM, not as a
standalone Nextor driver. In that case `USBETHER.COM` cannot discover the
`"MSXUSB"` UNAPI implementation through `EXTBIO`.

This variant builds a 128KB cartridge ROM with the MSXUSB Nextor driver image in
the visible first bank. That makes the `NEXTOR_DRIVER` signature visible to the
active Nextor kernel, allowing the driver to be initialized by the booting
environment and expose its `"MSXUSB"` UNAPI service through `DRV_EXTBIO`.

The source code for this firmware is kept separately in `src`. It started as a
copy of the current `NextorUsbHost` driver source, but the ext firmware builds
from its own files so changes here do not alter the bootable `NextorUsbHost`
firmware.

Intended layout:

```text
Slot 1: SDMapper, booting Nextor and providing DOS/storage/mapper support
Slot 2: MSXUSB flashed with NextorUSBHost_ext
Run:    USBETHER.COM from the SDMapper Nextor prompt
```

Build from `src`:

```sh
make
```

The flashable 128KB output is written to `dist/nextorusbhost_ext.rom`. The
intermediate 16KB Nextor driver bank is also kept as
`dist/nextorusbhost_ext_driver.rom` for inspection/debugging.

This package does not modify or replace the existing bootable `NextorUsbHost`
firmware.