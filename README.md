# picoFM
Raspberry Pi based VHF transceiver using the DR818V chip (SV1AFN board)

## Ham radio communications, a brief introduction

Ham radio designates as QRP transmission carrying low power (5W or less) and QRPp those that are even smaller, 1W or less.

As ham radio can legally use (depending on band privileges and categories of each license) up to 1.5 KW (1500 W) the
QRP power, and even further the QRPp power, looks as pointless and not usable.

This is far from the truth.  Really small power can be enough to sustain communications over great distances and therefore being a per se
limiting factor to communicate, however  there might other factors in the basic implementation which makes enables or makes it
difficult to communicate for other reasons. Antenna setup, transmission mode and propagation conditions can be counted among some
of these factors.

Great distances can be achieved using the HF spectrum (3 to 30 MHz) where most of the traditional ham bands are located; local coverage
can be achieved using VHF (30 to 300 MHz), UHF (300 to 3000 MHz) and Microwaves (3000 MHz and beyond) where QRP or less power is the norm
but essentially the communication happens within the, pretty much, observable horizon.

Bands below HF, such as medium frecuencies (300 KHz to 3 Mhz) or even lower (less than 300 KHz) requires power well above the QRP levels
and/or combined with very specialized antennas.

Having VHF as the target band for the project the distance that can be expected is about the radio horizon, which is fairly similar to the
visual horizon, therefore with higher antennas the range can be extended. A distance from 5 to 20 Km should be expected even with small power.

At the ham bands communications can be carried using different modes, which is the way the information is "encoded" in the signal. The
most obvious way is just talking, called Phone in ham parlance. In ham bands Phone communications is carried out using SSB (USB or LSB
depending on the band), in some higher bands narrow band FM such as this transceiver is also used (28 MHz) but this mode is fairly more popular in VHF and
beyond rather than HF. Phone conversations can also be carried using amplitude modulation (AM) like the one used by broadcast radios
but this mode, even if still used, is relatively inefficient for lower powers and trend not to be much used. Modes not carrying actual
voice are also fairly popular. The oldest and more traditional is CW where information is sent using Morse code, to many the most
efficient mode of communication since it requires very low powers to achieve long distance communications, this situation is actually
being changed with the introduction of Weak Signal Modes (such as FT8) where highly specialized coding techniques are used to carry
small amoount of informations using incredibly low powers and still been able to be decoded in the other side. The modes hams uses
for communications are many to be discussed in detail here, further information can be obtained from [here](http://www.arrl.org/modes-systems).

Finally, for a short introduction at least, an antenna component is required to carry ham radio communications. A very specialized
topic by itself and a field of endless experimentation. More information about this topic can be obtained from [here](http://www.arrl.org/building-simple-antennas).

In order to use amateur radio bands for listening the activity is free and without requirements, in most parts of the world at least,
check your local communications authorities requirements for the subject to find any condition to met. In the HF bands persons
doing that activities are close relatives of Hams, usually called SWL (Short Wave Listerners) and for many it's a very specialized
hobby, which by the way isn't incompatible with being a Ham at the same time. See [here](https://www.dxing.com/swlintro.htm).

To carry amateur radio comunications, the radioamateurs or "hams", needs to both receive and transmit at designated frequencies, which are called "ham bands",
although ham frequencies "allocations" are somewhat generic for that particular use, each communication authority at each band allow individuals to access
certain segments based on their license level or "privileges", typically associated with demonstrated technical ability, proof of mastery of certain aspects
of ham radio or age in the activity. Certains part of the bands are, at the same time, designated to carry specific modes in them; although the general
assignment is made at the global level each administration usually introduces a fine tuning on that usage based on their own spectrum management policies.
As an example ham radio frequency assignments or "band plan" for USA can be seen [here](http://www.arrl.org/band-plan) but remember to check the ones
specific for your country.

## Ham Radio FM Transceiver for the VHF band of 144-148 MHz

## PixiePi and OrangeThunder, Sister projects

A very similar project, based on the same foundations is named [PixiePi](http://www.github.com/lu7did/PixiePi), software
libraries created for it are used on this project, and viceversa. The main difference among projects is the
hardware used to implement the transceiver.

Main features and differences of PixiePi compared with OrangeThunder

* The receiver chain is a direct conversion receiver and the transmitter chain a CW (class C) amplifier, based on the DIY kit Pixie.
* It is based on a Raspberry Pi Zero board which is substancially less powerful than a Raspberry Pi 3 or 4 board.
* It is intended to be used pretty much as a stand alone transceiver as it lack resources to host a user and GUI usage.
* Requires a 9 to 12V power supply
* Mostly headed toward CW usage although other modes are supported.
* Optionally allows a local control with LCD display, tuning knob and other direct operation hardware.

## DISCLAIMER

This is a pure, non-for-profit, project being performed in the pure ham radio spirit of experimentation, learning and sharing.
This project is original in conception and has a significant amount of code developed by me, it does also being built up on top
of a giant effort of others to develop the base platform and libraries used.
Therefore this code should not be used outside the limits of the license provided and in particular for uses other than
ham radio or similar experimental purposes.
No fit for any purpose is claimed nor guaranteed, use it at your own risk. The elements used are very common and safe, the skills
required very basic and limited, but again, use it at your own risk.
Despite being a software enginering professional with access to technology, infrastructure and computing facilities of different sorts
I declare this project has been performed on my own time and equipment.


## Fair and educated warning

Raspberry Pi is a marvel.

Hamradio is the best thing ever invented.

¡So don't ruin either by using a transceiver to transmit without a proper license from your country's communications authority!

You can receive with to your heart's content tough.

Remember that most national regulations requires the armonics and other spurious outcome to be -30 dB below the fundamental which is achieved by
the SV1AFN board used in this project but the bare DRA818V chip might fail to be up to if the output isn't filtered properly.

                     *********************************************************
                     *  Unfinished - incompletely published code - WIP       *
                     *  No support, issues or requests can be honored until  *
                     *  the project stabilizes. Check for updates here       *
                     *********************************************************

## Pre-requisites

# Program usage

## Installation / update:

Download and compile code:
```
sudo rm -r /home/pi/picoFM
cd /home/pi
git clone https://github.com/lu7did/picoFM
cd picoFM/src
sudo make
sudo make install
```

## Prototype

![Alt Text](docs/picoFM_V2.0.jpg?raw=true "Hardware Prototype")

### 3D Printer STL files

Preliminary files, pending finalization and fine adjustment, can be found at

![Alt Text](docs/picoFM_V2.0.stl?raw=true "Hardware Prototype")

