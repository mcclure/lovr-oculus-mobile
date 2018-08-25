#!/usr/bin/python

import os
import sys

def init():
    root = os.path.realpath( os.path.dirname(os.path.realpath(__file__) ) )
    os.chdir(root) # make sure we are always executing from the project directory
    while( os.path.isdir( os.path.join(root, 'bin/scripts/build') ) == False ):
        root = os.path.realpath( os.path.join(root, '..') )
        if( len(root) <= 5 ): # Should catch both Posix and Windows root directories (e.g. '/' and 'C:\')
            print ('Unable to find SDK root. Exiting.')
            sys.exit(1)
    root = os.path.abspath(root)
    os.environ['OCULUS_SDK_PATH'] = root
    sys.path.append( root + "/bin/scripts/build" )

init()
import ovrbuild

ovrbuild.init()
# TODO: command options are currently not populated for the sdk_libs
# task, and therefore, the use daemon option was ignored. This may be
# the source of overtime issues on the build machines.
#
# Force to False for now until we can route through the
# command_options.
ovrbuild.command_options.use_gradle_daemon = False
ovrbuild.command_options.should_install = False
ovrbuild.command_options.disable_sig_check = True

def build_sdk_libs():
    try:
        ovrbuild.build_in_dir(os.environ['OCULUS_SDK_PATH'])
    except ovrbuild.NoSourceException as e:
        pass

build_sdk_libs()
