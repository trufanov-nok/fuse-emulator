import fuse

def dostuff(x):
    print('Argument: {}'.format(x))
    y = fuse.fn();
    print('Callback: {}'.format(y))
