# linux-keylogger
Threaded keylogging for multiple keyboard devices. Requires root when running to read keyboard device files.

#### Compile and Run
```
gcc -pthread -o keylog keylog.c
sudo ./keylog
```
#### Usage
In the example below "testing" was typed on a keyboard and a mouse button that is bound to Numpad 7 was pressed. Linux keycodes don't differentiate between uppercase and lowercase, assume everything is lowercase unless SHIFT or CAPSLOCK is used.

[![log1.png](https://i.postimg.cc/htmq6xNC/log1.png)](https://postimg.cc/21CJBqkv)

Using the bash command `find /dev/input/by-path/*-kbd` a list of files which store the output keyboard-like devices is found. Often there are multiple keyboard files due to mice with extra buttons being treated like a keyboard, multiple physical keyboards connected to the same system, etc.

This logger will listen to every keyboard device, outputting to multiple log files corresponding to each device. This means that every keyboard-like device is being listened to, instead of making a best-guess and only logging the primary keyboard.

#### Notes
- Numpad numbers are represented as (7), normal numbers just as 7
- If the log files already exist the new logs will be appened to the existing logs
- The output log files are given 0744 permissions by default
