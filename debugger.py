import fuse

def breakpoint(breakpoint_id):
    print('Now in Python code; breakpoint_id = {}'.format(breakpoint_id))
    memory = fuse.read_memory(0x4000, 0xc000)
    with open('memory.bin', 'wb') as f:
        f.write(memory)
    print('Back from read_memory')
