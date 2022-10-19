from setuptools import Extension, setup

mod_def = Extension(
    name='pyglfs',
    sources=[
        'src/pyglfs.c',
        'src/pyglfs-fd.c',
        'src/pyglfs-handle.c',
        'src/pyglfs-stat.c',
        'src/pyglfs-volume.c'
    ],
    libraries=[
        'gfapi',
        'bsd',
    ],
    include_dirs=[
        '/usr/include/glusterfs'
    ]
)

setup(
    name='pyglfs',
    version='0.0.1',
    ext_modules=[mod_def]
)
