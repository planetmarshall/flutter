# Copy this file to the root of your flutter checkout to bootstrap gclient
# or just run gclient sync in an empty directory with this file.
solutions = [
  {
    "custom_deps": {
        'engine/src/flutter/third_party/ktx': 'https://github.com/KhronosGroup/KTX-Software@0306d6a614ddde1b88c2a1b7d65186a20cdd4d25',
        'engine/src/flutter/third_party/skia': 'git@github.com:entos-xe/skia.git@84ab42015fe324875ce86282fc23809eddf7f256',
        'engine/src/flutter/third_party/swiftshader': 'git@github.com:entos-xe/SwiftShader.git@f9fb3bc1a7e68823ca63a0ca5ee9375583fd55b2'
        },
    "deps_file": "DEPS",
    "managed": False,
    "name": ".",
    "safesync_url": "",

    # If you are using SSH to connect to GitHub, change the URL to:
    # git@github.com:flutter/flutter.git
    "url": "git@github.com:entos-xe/flutter.git"

    # Uncomment the custom_vars section below if you plan to build the web engine.
    # "custom_vars": {
    #   "download_emsdk": True,
    # },
  },
]
