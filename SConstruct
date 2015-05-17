import os

import scons.utils

SetOption('num_jobs', scons.utils.detectCPUs())

VariantDir('build', 'src', duplicate=0)

includedirs = '{0}/src'.format(os.getcwd())

env = Environment(ENV = os.environ, CPPPATH=includedirs, CPPDEFINES = ['USE_ALLEGRO5'])

env.ParseConfig('pkg-config r-tech1 --libs --cflags')

source = ['src/argument.cpp',
        'src/game.cpp',
        'src/main.cpp']

env.Append(CXXFLAGS = ['-g3'])

env.Program('asteroids', [s.replace('src', 'build') for s in source])
