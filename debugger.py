import fuse

def dostuff(breakpoint):
    print('Breakpoint: {}'.format(breakpoint))
    y = fuse.fn(42);
    print('Callback: {}'.format(y))
