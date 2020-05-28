all: ../bin/picoFM 

CCP  = c++
CC   = cc
GCC  = gcc

CFLAGS  = -Wall -g -O3 -Wno-unused-variable -lrt -lpthread -lpigpio -I$(INCLUDEDIR) -I/usr/include/libusb-1.0 
LIBRPITX = /home/pi/librpitx
CXYFLAGS = -std=c++11 -Wall -g -O3 -Wno-unused-variable -DLIBCSDR_GPL -DUSE_FFTW -DUSE_IMA_ADPCM -I$(INCLUDEDIR) -I/usr/include/libusb-1.0 
LDFLAGS = -lm -lrt -lpthread -lwiringPi -lpigpio -L$(LIBDIR) 

LIBDIR=/usr/local/lib
PP   = /home/pi/PixiePi/src
OT   = /home/pi/OrangeThunder/src
INCLUDEDIR=/usr/local/include

CXXRPITX = -std=c++11 -Wall -g -O3 -Wno-unused-variable
LDRPITX = -lm

CXXFLAGS = -std=c++11  -g -O2 -Wno-unused-variable

CXXFLAGS = -std=c++11 -Wall -g -O2 -Wno-unused-variable -DLIBCSDR_GPL -DUSE_FFTW -DUSE_IMA_ADPCM -I$(INCLUDEDIR) -I/usr/include/libusb-1.0 

OSC_CFLAGS=-DOSCILLATOR_Z -DOSCILLATOR_D


../bin/picoFM : picoFM/picoFM.cpp picoFM/picoFM.h
	$(CCP) $(OSC_CFLAGS) $(CXYFLAGS) -o ../bin/picoFM picoFM/picoFM.cpp  $(LDFLAGS)

clean:
	rm -r  ../bin/picoFM

install: all
	install -m 0755 ../bin/picoFM  /usr/bin

 
