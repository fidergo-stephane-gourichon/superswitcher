# superswitcher

## History

After Nigel Tao appeared to stop maintaining superswitcher (introduced
below), I continued to use it (from ca 2012 on), and make necessary
adjustments to that it compiles on newer distributions.  The result is
shared on <https://github.com/fidergo-stephane-gourichon/superswitcher>.

The document below and the code here were published by Nigel Tao on
<https://github.com/nigeltao/superswitcher> before being adjusted.

## Introduction

SuperSwitcher is a (more feature-ful) replacement for the Alt-Tab window
switching behavior and Ctrl-Alt-Left/Right/Up/Down workspace switching behavior
that is currently provided by Metacity.

## How to use it

See original [README](README) file.

(TODO move content here, rearrange for clarity.)

## Building, Installing, Running.

### As pure user (no root access)

#### Run and test drive

Prefix here is the directory where things will be installed.

It can be a dedicated directory, which has the benefit that uninstalling is little more than removing the whole directory altogether.

    PREFIX=~/any/persistent/directory/where/you/can/write

If you don't know what to pick, you can use `~/.local`.  It is probably standards-compliant, although I'm not sure to what extent.

    PREFIX=~/.local/

Anyway, after setting `PREFIX` to what you want, do this:

    ./autogen.sh --prefix="${PREFIX:?}"

    make

After that, you can run src/superswitcher immediately for a test.

### User-level installation

Previous step, plus:c

    make install

### Have it automatically run on your user sessions

SuperSwitcher is best enjoyed if you automatically start the program
whenever you log in.

Assuming you use a XDG-compliant desktop environment (pretty much any, noawadays).

It is best to use your desktop environment's tools but this should work perfectly:

    cp -f --backup=numbered "${PREFIX:?}"/share/applications/SuperSwitcher.desktop ~/.config/autostart/

(Remember to have `PREFIX` defined in the shell you use.)

### No longer have it automatically run on your user sessions

    rm ~/.config/autostart/SuperSwitcher.desktop

### Run and test drive without install

To build for a system-level install as root:

    ./autogen.sh --prefix=/usr
    make

After that, you can run src/superswitcher immediately for a user-level test.

### System-level installation

    sudo make install

### Have it automatically run on all users's sessions

Assuming you use a XDG-compliant desktop environment (pretty much any, noawadays).

    sudo cp -f --backup=numbered /usr/local/share/applications/SuperSwitcher.desktop /etc/xdg/autostart/

### No longer have it automatically run on all users's sessions

    sudo rm /etc/xdg/autostart/SuperSwitcher.desktop

### Debian/Ubuntu package

TODO

## Other topics

See original [README](README) file.
