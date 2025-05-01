# superswitcher

## History

After Nigel Tao appeared to stop maintaining superswitcher (introduced
below), I continued to use it (from ca. 2012 on), and still make necessary
adjustments so that it compiles on newer distributions.  The result is
shared on <https://github.com/fidergo-stephane-gourichon/superswitcher>.

The document below and the code here were published by Nigel Tao on
<https://github.com/nigeltao/superswitcher> before being adjusted.

## Introduction

SuperSwitcher is a (more featureful) replacement for the Alt-Tab window
switching behavior and Ctrl-Alt-Left/Right/Up/Down workspace switching behavior
that is currently provided by Metacity.

## How to use it


When running, use the "Super" key (also known as the "Windows" key) to switch
between windows and workspaces.  This key is usually found between the Ctrl and
Alt keys on the keyboard.

Super-Tab works just like Alt-Tab does (with and without Shift), but also:

Super-Up and Super-Down cycles through all windows in the current workspace in
a fixed order (as opposed to Alt-Tab or Super-Tab, whose list of windows is
ordered by most-recently-used first - which is good if you want recently used
windows, but it's clumsy to cycle through, for example, all three or four
windows in the one workspace).  Hold down Shift to re-order the list instead of
switching between windows.

Super-PageUp and Super-PageDown maximize and minimize the active window (or
restores them if it was already maximized or minimized).  Super-Ctrl-PageUp and
Super-Ctrl-PageDown do this to all windows on the current workspace, not just
the active one.

Super-Left and Super-Right cycles through your workspaces.  Hold down Shift
to also bring across the active window into the new workspace.  Hold down Shift
and Ctrl to bring across all windows from one workspace to the next.  Unlike
Metacity's Ctrl-Alt-Left and Ctrl-Alt-Right, this "loops" so that, when you get
to the end of the list, you cycle back to the start.

Super-F1 moves to workspace number 1, Super-F2 moves to workspace number 2, and
so on, up to Super-F12.  Again, hold down Shift to bring across the active
window, and both Shift and Ctrl to bring across a flock of windows, just like as
described above.

Super-Insert creates a new, empty workspace.  Again, hold down Shift to also
bring across the active window (if there is one) into the new workspace.

Super-Delete deletes the current workspace, if it is empty.  Super-Shift-Delete
will delete all empty workspaces, down to a minimum of one.

Super-Escape closes the active window.  Super-Ctrl-Escape will close all windows
on this workspace.

Whilst holding down Super, typing regular letters or numbers will show you only
those windows whose titles match that pattern.  Pressing Enter (whilst still
holding down Super) will then cycle through the matches, regardless of which
workspace you are on.  For example, if you have a lot of windows open, and you
want to get to your web browser window that is showing planet.gnome.org, then
hold down Super, then type "p" "l" "a" "Enter" and then release the Super key.
Shift-Enter goes in the other direction than Enter (i.e., up and left with
Shift, versus down and right without Shift).  Use the Space key to enter
multiple word fragments, such as "pla gn", to further refine your search.

Finally, when holding down Super, click on image or text representing a window
or a workspace to activate it.

(For more details see original [README](README) file.)1

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

Previous step, plus:

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

From this repository tree, you can easily build your own binary package for Ubuntu (not 24.04 and later at the moment) or Debian, using the supplied `recompile_local_debian_package.sh` script.  If that doesn't work it's a bug and I'll be happy to hear about it.

You probably needs something like this:

```
sudo apt install build-essential dpkg-dev debhelper git build-essential devscripts fakeroot lsb-release
sudo apt install libwnck-dev # libwnck-3-dev in recent distro
bash recompile_local_debian_package.sh
```

The script will output a list of generated files, including a `.deb` file which is the compiled binary package, which you can install with e.g. `sudo dpkg --install /full/path/to/superswitcher_0.9.3-1_amd64.deb`.

## Other topics

See original [README](README) file.
