# minireef

Minireef is a minimal reverse port forwarding implementation for Android. It works without root when run via [ADB](http://developer.android.com/tools/help/adb.html) and should work on pretty much any SDK level.

## Building

Building requires [NDK](https://developer.android.com/tools/sdk/ndk/index.html), and is known to work with at least with NDK Revision 10 (July 2014).

Then it's simply a matter of invoking `ndk-build`.

```
ndk-build
```

You should now have the binaries available in `./libs`.

## Usage

One process can only handle a single reverse forwarded port. In order to have multiple ports open, multiple instances of the program must be launched, each handling one port.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

See [LICENSE](LICENSE).

Copyright © CyberAgent, Inc. All Rights Reserved.
