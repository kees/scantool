# USAGE NOTES with Ubuntu 20.04 LTS

## Configuration file

*ScanTool* saves the settings of the configuration panel in a file named *.scantoolrc* located in the home directory. Example of configuration file:

```ini
version = 2.1

[general]
system_of_measurements = 0
display_mode = 1

[comm]
baud_rate = 15
comport_number = 4

[sensors]
sensor0 = -1
sensor1 = -1
sensor2 = -1
sensor3 = -1
sensor4 = -1
sensor5 = -1
sensor6 = -1
sensor7 = -1
sensor8 = -1
sensor9 = 0
sensor10 = 0
sensor11 = 0
sensor12 = 0
sensor13 = 0
sensor14 = -1
sensor15 = 0
sensor16 = 0
sensor17 = 0
sensor18 = 0
sensor19 = 0
sensor20 = 0
sensor21 = 0
sensor22 = 0
sensor23 = 0
sensor24 = 0
sensor25 = 0
sensor26 = -1
sensor27 = 0
sensor28 = 0
sensor29 = 0
sensor30 = 0
sensor31 = 0
sensor32 = 0
sensor33 = 0
sensor34 = 0
sensor35 = 0
sensor36 = 0
sensor37 = 0
sensor38 = 0
sensor39 = 0
sensor40 = 0
sensor41 = 0
sensor42 = 0
sensor43 = 0
sensor44 = 0
sensor45 = 0
sensor46 = 0
sensor47 = 0
sensor48 = 0
sensor49 = 0
sensor50 = 0
sensor51 = 0
sensor52 = 0
sensor53 = 0
sensor54 = 0
sensor55 = 0
sensor56 = 0
sensor57 = 0
sensor58 = 0
sensor59 = 0
sensor60 = 0
sensor61 = 0
sensor62 = 0
sensor63 = 0
sensor64 = 0
sensor65 = 0
sensor66 = 0
sensor67 = 0
sensor68 = 0
sensor69 = 0
sensor70 = 0
sensor71 = 0
```

To reset the configuration (e.g., when the program is stuck in resetting an unavailable hardware interface), simply remove the file:

```bash
rm ~/.scantoolrc
```

## Standard serial COM port

A standard serial COM port in the form `/dev/ttyS<number>` is configured in *~/.scantoolrc* with the following line: `comport_number = <number>`; for instance, `/dev/ttyS3` is represented as follows in *~/.scantoolrc*:

```ini
[comm]
comport_number = 3
```

## USB COM port

A USB COM port in the form `/dev/ttyUSB<number>` is configured in *~/.scantoolrc* with the following line: `comport_number = ` *100 + USB number*; for instance, for */dev/ttyUSB0*, set the following in *~/.scantoolrc*:

```ini
[comm]
comport_number = 100
```

## Pseudo-tty support

To use a pseudo-tty as COM port, edit *~/.scantoolrc* and add the line `comport_number = ` *1000 + pty number*; for instance, for */dev/pts/3*, set the following in *~/.scantoolrc*:

```ini
[comm]
comport_number = 1003
```

Note: when setting a pseudo-tty, only edit *~/.scantoolrc* and do not try setting a PTY serial port through the *"Options"* panel of the user interface, which currently does not provide this feature.

## Compilation

To perform a standard compilation, set the *RELEASE* flag to *YES*. Check the following example, which also includes prerequisites:

```bash
sudo apt install -y liballegro4.4 liballegro4-dev allegro4-doc
make clean
make -e RELEASE=yes
```

### Enabling logging

In order to enable logging to a file named *"comm_log.txt"* in the current directory, set the *LOG* compilation flag to *YES*, like in the following example:

```bash
make clean
make -e RELEASE=yes LOG=yes
```

### Compilation in DEBUG mode

To compile in debug mode, set the *DEBUGMODE* compilation flag to *YES* instead of using the *RELEASE* flag, like in the following example, which also includes logging to *"comm_log.txt"*:

```bash
make clean
make -e DEBUGMODE=yes LOG=yes
```
