import fuse

def breakpoint(breakpoint_id):
    print('Now in Python code; breakpoint_id = {}'.format(breakpoint_id))
    if breakpoint_id == 1:
        print('Writing to memory...')
        attributes = bytes([8] * 768)
        fuse.write_memory(22528, attributes)
        print('Bytes written')
    elif breakpoint_id == 2:
        print('Reading memory...')
        attributes = fuse.read_memory(22528, 768)
        print('Memory read')
        with open('attributes.bin', 'wb') as f:
            f.write(attributes)
        print('File dumped')

    print('Exiting Python code')
