# MSX-USB
## Some history
In the 80's the MSX standard was created by Kazuhiko Nishi. He envisioned a simple yet powerful home computer based on standard hardware, software (BIOS/MSX DOS) and connectivity. From 1983 to 1990 manufacturers, like Philips, Sony, Yamaha, Sharp, Canon and many more, made computers according to this standard.

Because of the fact that extensibility was part of the standard people could design cartridges that would add synthesizer sound, memory, modems, rom-based games, or combinations of the above.

Cartridges were connected via a standard 50pin connector that remained the same accross 4 generations of MSX (1/2/2+/TurboR). BIOS and MSX-DOS were designed in a way to discover these via rom-based drivers, software hooks, extended bios.

Standardisation offers a big advantage because everything that adheres to the standard can connect to it.

## USB
USB was designed in the 90's to do something similar. To standardize the connection of peripherals to personal computers, both to communicate with and to supply electric power. It has now largely replaced interfaces such as serial ports and parallel ports, and has become commonplace on a wide range of devices. Like USB flash drives, keyboards, mice, wifi, serial, camera's, etc.

What if the MSX would have an USB connection? And if the BIOS/MSX DOS would automatically recognize devices connected to it?

## Meet MSX-USB
Inspired by the work of Xavirompe and his [RookieDrive](http://rookiedrive.com/en/) S0urceror set out to do build an open source project that would add USB to the MSX. The project is called MSX-USB and is hosted on GitHub. 

The project had contributions of many people in the MSX community. With people contributing to improve the hardware, firmware, drivers, and documentation.

On this GitHub page you can find:
* Schematics (KiCAD), that interfaces a cheap USB CH376s board to the MSX cartridge port.
* Drivers, currently Flash drives on Nextor and HID keyboards are supported.
* Debugging tools, to connect a test board to your PC/MAC and assist in debugging.

## Hardware

This project connects the CH376s board you can find on eBay or AliExpress for around 4 USD to the MSX.  The CH376s board is a USB host controller that can handle USB devices. The board is connected to the MSX cartridge port and is controlled by a CPLD. The CPLD is programmed to handle the MSX bus protocol and to interface with the CH376s board.

Circuitry is added to correctly handle the _BUSDIR signal. Make sure you select parallel by setting the jumper on the CH376s board correctly.

The CH376s board is accessible via ports 10h and 11h on the MSX, where port 11h serves as the command port and port 10h as the data port. 

Programming details for the CH376s can be found in various online resources:
* [CH376s Datasheet](https://www.mpja.com/download/ch376ds1.pdf)
* [Arduino Basics: CH376s USB Read/Write Module](https://arduinobasics.blogspot.com/2015/05/ch376s-usb-readwrite-module.html)

Most information is available on how to use the higher-order API for flash drives. 

If you want to use other USB devices you have to go low-level. As it turns out Konamiman already did some work there and after some researching I got USB HID Keyboards, Ethernet, Serial working as well. Check out my source-code or these great pages for more information:

* http://www.usbmadesimple.co.uk/index.html
* https://www.beyondlogic.org/usbnutshell/usb1.shtml

### PCB

If you prefer using PLCC versions of the CPLD and flash memory chips, it is recommended to use the v4 PCB by @cristianoag. This version fits into a Konami-style cartridge case and includes a switch to toggle between two ROM images. Be sure to use the rev4 CPLD code to enable this functionality. You can find the Gerber files and CPLD code for v4 at these links: [Gerber Files](hardware/v4/kicad-cpld-rev4/production) and [CPLD Code](hardware/v4/quartus-rev4).

If you’re comfortable with SMD soldering and want to avoid the challenges of sourcing 7064SLC without JTAG locks, use the v5 CPLD code and PCBs instead (also created by @cristianoag). The files for these are available at: [CPLD Code](hardware/v5/cpld) and [PCB Files](hardware/v5/kicad). The v5 CPLD verilog code also includes support to the additional 20h and 21h ports so enabling the use of new drivers that were developed for those ports by Konamiman.

Please note that for v5 there are two folders for the PCBs because there are different types of CH376 modules available. Depending on the module you have, you’ll need to use a specific version of the PCB. Compare the signals of your module with those on the PCBs to ensure you select the correct one.

**Very important:** The MSX-USB project requires modules with the CH376S firmware with at least version 0x43 (Version 3). If you have a CH376 module with a CH376S chip with versions 0x41 or 0x42 the drivers will not work. You can check the version of the firmware by issuing a CMD_GET_IC_VER command to the CH376S chip. Also some of the drivers display the version of the CH376S firmware when they are loaded.

## BIOS and Drivers

The project has several bios and drivers that have been developed. You can use a flash programmer or the MSX itself to flash the bios options to the flash chip on the cartridge. To do that use the flash.com tool that is available in the software folder. 

The flash.com command has the following syntax:

```
flash.com <filename>.rom
```

The [flash.com tool](software/flash/dist/flash.com) supports AMC_A29040B (AMIC A29040B), AMD_AM29F040, AMD_AM29F010 and SST_SST39SF040 chips. It detects the chip and selects the appropriate identification, erase and programming commands. Other chips require an external flash programmer.

Use `flash.com /S1 <filename>.rom` (or `/S0`, `/S2`, `/S3`) to select a primary slot instead of scanning all four slots. The selected slot is still checked for a supported chip before erasing. The tool erases the entire flash chip, not just the space occupied by the ROM file.

SST39SF040 commands use flash addresses `0x5555` and `0x2AAA`, rather than the shorter AMD/AMIC unlock addresses. The tool maps dedicated 8 KB command banks and temporarily replaces page 2 RAM while issuing SST commands, restoring RAM before accessing the file buffer or DOS. SST detection requires the stack to be in page 3, with at least 256 bytes available above `0xC000`. This requires the MSX-USB mapper's `5x00`/`7x00`/`9x00` bank-register decoding, as implemented by the v3, v4 and v5 CPLD sources. On v4, select the first ROM bank with the hardware switch so flash address zero is accessible for SST identification.

The host-side flash regression tests can be run with `make -C software/flash test` (Node.js and a C++17 `g++` compiler are required; `CXX` can select another compatible compiler). They exercise the actual C protocol and command-line code against a simulated flash chip and cartridge mapper; they do not replace testing on physical hardware. SST command and polling behavior follows the [Microchip datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/SST39SF010A-SST39SF020A-SST39SF040-Data-Sheet-DS20005022.pdf).

### USB Host BIOS (USBHOST.ROM)

S0urceror wrote a UNAPI USB specification and implemented the Usb Host driver according to it. The next version of this Host driver will also implement the Usb Hub specification and enumerate and initialise all devices connected.

* **USB HID Keyboard Driver** The Usb Keyboard driver is a program that connects to Unapi Usb driver and hooks itself to H.CHGE. From that moment on it replaces your trusted MSX keyboard by a shiny new USB Keyboard. Or a wireless one if you have inserted the appropriate Logitech receiver.

* **USB CDC ECM Ethernet Driver** The USB CDC ECM Ethernet driver is finished. It uses the Unapi USB and conforms to the Unapi Ethernet standard. Internestor Lite can now connect and use your USB Ethernet device. Please note that currently we only support USB CDC ECM. Make sure your Ethernet device supports this. All USB Ethernet devices built around the **RTL8153** chipset support USB CDC ECM. They usually cost around 20 euro.

### USB Storage NEXT (USBNEXT.ROM)

This is a firmware developed in C that implements a menu that is capable to mount DSK files or boot the computer from a connected USB stick. It also is a Nextor driver that allows you to use the USB stick as a hard disk. 

You can choose between (1) to initialize via floppy disk, (2) to initialize via USB stick, or use letters correspondent to files and folders located on the root of the USB stick connected to the cartridge to navigate and mount DSK files.

This BIOS allows mounting DSK files that are bigger than 720KB, respecting the limitations imposed by the Nextor OS.

### Konamiman USB FDD Firmware (USBFDD.ROM)

Konamiman developed a BIOS that allows you to mount DSK 360K/720K images stored on a USB floppy drive and use them as if they were real floppy disks. This version also allows you to boot the computer from a connected USB floppy drive compatible with the UFI standard.

You can check the [Konamiman's Floppy Disk Controller ROM](https://github.com/Konamiman/RookieDrive-FDD-ROM) GitHub repository for more information. He also provides a full manual page describing how to use the BIOS in the [Disk Image Mode](https://github.com/Konamiman/RookieDrive-FDD-ROM/blob/master/DISK_IMAGE_MODE.md).

@cristianoag adapted the original BIOS to work with the MSX-USB project. You can find the adapted version in the [MsxUSBFDD](/drivers/MsxUSBFDD) folder. 

## Installation instructions
Check [this page](INSTALL.md) for installation instructions and links to the various binaries that have been developed.

## Build your own (DIY)
Check [this page](DIY.md) for information on how to make the PCB, program the CPLD device, flash the ROM, etc.

## Collaboration
Interested in contributing to the MSX-USB project? Whether you want to develop drivers for additional devices or assist in other areas, your help is welcome. Clone the repository and start contributing by filing issues, submitting pull requests, or sharing your ideas.

## License

![Open Hardware](images/ccans.png)

This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).

* If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
* You may not use the material for commercial purposes.
* You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.

**ATTENTION**

This project was made for the retro community and not for commercial purposes. So only retro hardware forums and individual people can build this project.

THE SALE OF ANY PART OF THIS PROJECT WITHOUT EXPRESS AUTHORIZATION IS PROHIBITED!