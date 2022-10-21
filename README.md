py-glfs
======

**Python bindings for libgfapi**

Minimal set of python bindings for libgfapi


**QUICK HOWTO:**

`import pyglfs`

Attach to glusterfs volume:
`vol = pyglfs.Volume(volume_name='ctdb_shared_vol', volfile_servers=[{'host': '127.0.0.1', 'proto':'tcp', 'port': 0}])`

Open a handle on "/" in glusterfs volume:
`root = a.get_root_handle()`

Open handle on 'nodes' file under `/`
`nodes = root.lookup('nodes')`

Open second handle based on uuid of existing handle:
`nodes2 = vol.open_by_uuid(nodes.uuid)`

Get glfs_fd handle from an open glfs handle:
`fd = nodes.open(os.O_RDWR)`
