# LXR-Free
Free edition of LXR, the Ultimate Light Detection plugin for Unreal Engine.<br>
MIT Licensed.

## Limitations
This free version does not include advanced LXR features which are:
- [Sense](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Sensing)
- [Memory](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Memory)
- [MethodObject](https://docs.clusterfact.games/docs/LXR/Guides/Setup/MethodObject)
- [Octree](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Octree)
- Multithread

LXRFree can't be used with [LXR-Examples](https://github.com/zurra/LXR-Examples)

## Usage
Add all files to your project's `Plugins/LXRFree` folder, make one if it does not exists.

Almost all of the guides on 
- [Emitting](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Emitting) ([Source](https://docs.clusterfact.games/docs/LXR/Classes/Source)) 
- [Detecting](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Detecting)([Detection](https://docs.clusterfact.games/docs/LXR/Classes/Detection))
<br> can be applied to LXRFree.

1. Add Detection Component to actor who wished to detect light
2. Add Source Component to light sources.

[Debug widget](https://docs.clusterfact.games/docs/LXR/Guides/Setup/Widget) for showing color and intensity is included.
Simple usage is to create a `WB_LxrCanvas` widget and it should automatically work.

Check [Examples](https://docs.clusterfact.games/docs/LXR/Guides/Examples) for some example usages.

Support [discord](https://discord.gg/aWvgSa9mKd)

