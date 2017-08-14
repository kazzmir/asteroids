import os

import scons.utils

def getDebug():
    try:
        return int(os.environ['DEBUG'])
    except KeyError:
        return 0

SetOption('num_jobs', scons.utils.detectCPUs())

VariantDir('build', 'src', duplicate=0)

includedirs = '{0}/src'.format(os.getcwd())

env = Environment(ENV = os.environ, CPPPATH=includedirs, CPPDEFINES = ['USE_ALLEGRO5', 'DEBUG' if getDebug() else ''])

env.ParseConfig('pkg-config {0} zlib --libs --cflags'.format('r-tech1-debug' if getDebug() else 'r-tech1'))

source = ['src/argument.cpp',
        'src/game.cpp',
        'src/main.cpp']

env.Append(CXXFLAGS = ['-g3'])

env.Program('asteroids', [s.replace('src', 'build') for s in source])
