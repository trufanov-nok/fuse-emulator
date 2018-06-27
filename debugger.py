import fuse

def breakpoint(breakpoint_id):
    print('Now in Python code; breakpoint_id = {}'.format(breakpoint_id))
    fuse.save_binary(0x4000, 0xc000, 'memory.bin');
    print('Back in Python code')
