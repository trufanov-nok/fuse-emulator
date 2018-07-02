import fuse

def breakpoint(breakpoint_id):
    print('Now in Python code; breakpoint_id = {}'.format(breakpoint_id))
    if breakpoint_id == 1:
        attributes = bytes([8] * 768)
        fuse.write_memory(22528, attributes)
    elif breakpoint_id == 2:
        attributes = fuse.read_memory(22528, 768)
        with open('attributes.bin', 'wb') as f:
            f.write(attributes)
        print('File dumped')
    else:
        print('Unknown breakpoint {}'.format(breakpoint_id))
        return

    fuse.run()
