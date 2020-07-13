# minirev

# Warning

This repository was superseded by https://github.com/DeviceFarmer/minirev

Minirev is a minimal reverse port forwarding implementation for Android. It works without root when run via [ADB](http://developer.android.com/tools/help/adb.html) and should work on pretty much any SDK level.

## Building

Building requires [NDK](https://developer.android.com/tools/sdk/ndk/index.html), and is known to work with at least with NDK Revision 10 (July 2014).

Then it's simply a matter of invoking `ndk-build`.

```
ndk-build
```

You should now have the binaries available in `./libs`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

See [LICENSE](LICENSE).

Copyright © CyberAgent, Inc. All Rights Reserved.
