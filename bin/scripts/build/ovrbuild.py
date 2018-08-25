#!/usr/bin/python
import os
import locale

import sys
import zipfile
import argparse
from subprocess import Popen, PIPE
from sys import exit, argv
from shutil import rmtree
import shutil
from xml.dom import minidom
from time import sleep
import util
import hashlib
import time
import shlex

class CommandOptions:
    def __init__(self, args):
        self.should_clean = args.type == 'clean'
        self.is_debug_build = args.type == 'debug'
        self.is_retail_build = args.type == 'retail'
        self.should_install = args.should_install and not self.should_clean
        self.clear_logcat = args.clear_logcat
        self.loglevel = args.loglevel
        self.profile = args.profile
        self.use_gradle_daemon = args.use_gradle_daemon
        self.disable_sig_check = args.disable_sig_check
        self.keystore_path = args.keystore_path
        self.keystore_pswd = args.keystore_pswd
        self.keyalias = args.keyalias
        self.keyalias_pswd = args.keyalias_pswd

    @classmethod
    def parse(cls, argv):
        args = cls.parser.parse_args(argv)
        validate_ok, validation_msg = cls.validate(args)
        if not validate_ok:
            if validation_msg is not None:
                sys.stderr.write(validation_msg)
            cls.parser.print_help()
            exit(1)
            return None
        return CommandOptions(args)

    @classmethod
    def validate(cls, args):
        if args.type == "retail":
            if None in (args.keystore_path, args.keystore_pswd, args.keyalias, args.keyalias_pswd):
                return (False, 'ERROR: When building "retail", --keystore_path, --keystore_pswd, --keyalias,'
                               ' --keyalias_pswd are required.\n')
        return (True, None)

    parser = argparse.ArgumentParser(description='Build a project and its dependencies')
    parser.add_argument(
        'type',
        type=str,
        help='the type of build',
        nargs='?',
        default='release'
        )
    parser.add_argument(
        '-n',
        help="don't install the built APK",
        dest='should_install',
        action='store_false'
        )
    parser.add_argument(
        '-c',
        help="clear logcat before running the app",
        dest='clear_logcat',
        action='store_true'
        )
    parser.add_argument(
        '-log',
        type=str, 
        help="specify gradle log level [quiet,lifecycle,info,debug]",
        dest='loglevel',
        default='quiet'
        )
    parser.add_argument(
        '-p',
        help="run gradle with profiling enabled",
        dest='profile',
        action='store_true'
        )
    parser.add_argument(
        '--no-daemon',
        help="don't use the gradle daemon when building",
        dest='use_gradle_daemon',
        action='store_false'
        )
    parser.add_argument(
        '--disable-sig-check',
        help="disables the check for oculussig file",
        dest='disable_sig_check',
        action='store_true'
        )
    parser.add_argument(
        '--keystore_path',
        type=str,
        help="The path to the keystore used for signing",
        dest='keystore_path',
        action='store',
        )
    parser.add_argument(
        '--keystore_pswd',
        type=str,
        help="The password for the keystore",
        dest='keystore_pswd',
        action='store',
        )
    parser.add_argument(
        '--keyalias',
        type=str,
        help="The private key used for signing",
        dest='keyalias',
        action='store',
        )
    parser.add_argument(
        '--keyalias_pswd',
        type=str,
        help="The password for the private key",
        dest='keyalias_pswd',
        action='store',
        )

class BuildFailedException(Exception):
    pass

class NoSourceException(Exception):
    pass

# 'unicode' is not present with python 3.x
try:
    STRING_TYPES = [ str, unicode ]
except:
    STRING_TYPES = [ str, ]

def build_command_list(cmdline, shell):
    """
    Returns a list of command-line arguments for subprocess.Popen

    If cmdline is already an array of arguments, the returned array is just
    encoded for the current shell character set. If cmdline is a string,
    the arguments are split along whitespace boundaries then encoded.

    :param cmdline: A string or array of command line arguments
    """
    if not cmdline:
        return []
    encoding = locale.getpreferredencoding() if shell else 'utf-8'
    cmds = shlex.split(cmdline) if type(cmdline) in STRING_TYPES else cmdline
    return map(lambda x: str(x).encode(encoding, 'ignore'), cmds)

def call( cmdline, targetDir=".", suppressErrors=False, grabStdOut=False, verbose=True ):
    useShell = os.name != "posix"
    cmds = build_command_list(cmdline, useShell)

    with util.chdir(targetDir):
        if verbose:
            print( ' '.join(map(bytes.decode,cmds)))
        if grabStdOut:
            p = Popen( cmdline, stdout=PIPE, stderr=PIPE, shell=useShell )
        else:
            p = Popen( cmdline, stderr=PIPE, shell=useShell )
        (out, err) = p.communicate()
        if grabStdOut and verbose:
            print(out)
        if not p.returncode == 0:
            # if this is not a source build, there will be no 'assemble' task for the root gradle to complete, so it
            # will throw an exception.  Rather than have the script determine whether a source build is necessary before
            # executing the command, we choose to run it anyway and catch the exception,
            gradleTask = "clean" if command_options.should_clean else "assembleDebug" if command_options.is_debug_build else "assembleRelease"
            if ("Task '%s' not found in root project 'OculusRoot'" % gradleTask) in err:
              raise NoSourceException( targetDir )
            error_string = "command (%s) failed with returncode: %d" % (cmdline, p.returncode)
            if verbose:
                print(err)
            if suppressErrors:
                print(error_string)
            else:
                raise BuildFailedException(error_string)

    return ( p.returncode, out, err )

def check_call( cmdline ):
    try:
        call( cmdline, suppressErrors=False, grabStdOut=True, verbose=False )
        return True
    except Exception as e:
        return False

def init(options_parser = CommandOptions.parse):
    global command_options
    command_options = options_parser(sys.argv[1:])

    ndk_envars = [ 'ANDROID_NDK', 'NDKROOT', 'ANDROID_NDK_HOME' ]
    sdk_envars = [ 'ANDROID_HOME' ]

    # Check to see if we have the right tools installed.
    if not (any(map(lambda x: os.environ.get(x), ndk_envars)) or check_call(["ndk-build", "--version"])):
        print("ndk-build not found! Make sure ANDROID_NDK_HOME is set for command line builds")
    if not (any(map(lambda x: os.environ.get(x), sdk_envars)) or check_call(["adb", "version"])):
        print("adb not found! Make sure ANDROID_HOME is set for command line builds")

def gradle_command():
    # Use the wrapper so people don't need to install Gradle
    # Handle directory structure types for building from an app project and building
    # from within a lib project
    paths = ['.',  '../../', '../../../', '../../../../', '../../../../../']
    scriptdir  = os.path.realpath( os.path.dirname(os.path.realpath(__file__) ) )
    for path in paths:
        if os.path.exists(os.path.join(scriptdir, path, 'gradlew')):
            return os.path.join(scriptdir, path, 'gradlew')
    return None

def find_gradle_root_project():
    # Handle both directory structure types: ones with Projects/Android and ones without
    paths = ['.', '../../', '../../../']
    for path in paths:
        # settings.gradle files indicate a Gradle app (or 'root project')
        # whereas build.gradle files indicate a Gradle project/module that
        # typically the app will pull in via settings.gradle.
        if os.path.exists(os.path.join(path, 'settings.gradle')):
            return os.path.join(path, 'build.gradle')
    return None

def run_gradle_task(opts, task, args = None):
    """
    Forks a sub-process to execute a gradle build task

    :param opts: Parsed command line options
    :param task: Gradle task name
    :param args: Array of additional arguments to supply to gradle build
    """
    flags = [task]
    flags.append('--daemon' if opts.use_gradle_daemon else '--no-daemon')
    # lifecycle logging is enabled when a log level is not specified.
    if opts.loglevel != "lifecycle": flags.append('-%s' % opts.loglevel)
    if opts.profile: flags.append('--profile')
    if opts.disable_sig_check: flags.append('-Pdisable_sig_check')
    if opts.clear_logcat: flags.append('-Pclear_logcat')

    gradle_file_path = find_gradle_root_project()
    with util.chdir(os.path.dirname(gradle_file_path)):
        beginTime = time.time()
        command = [ gradle_command() ] + flags + (args or [])
        call(command)
        endTime = time.time()
        deltaTime = endTime - beginTime
        print("Gradle took %f seconds" % deltaTime)

def build_in_dir( targetDir, args = []):
    with util.chdir(targetDir):
        print('\n\nbuilding in ' + targetDir)
        if os.path.exists( 'build.gradle' ):
            if command_options.should_clean:
                run_gradle_task(command_options, 'clean')
            elif command_options.is_debug_build:
                run_gradle_task(command_options, 'assembleDebug', args)
            else:
                run_gradle_task(command_options, 'assembleRelease', args)
        print('\n\nfinished building in ' + targetDir)

def build():
    try:
        # print gradle version
        run_gradle_task(command_options, '--version')
        # print ndk version
        print( "ANDROID_NDK_HOME: %s" % os.environ.get('ANDROID_NDK_HOME') )

        # set flags for build
        flags = []
        if command_options.should_install: flags.append('-Pshould_install')
        if command_options.keystore_path: flags.append('-Pkey.store=%s' % command_options.keystore_path)
        if command_options.keystore_pswd: flags.append('-Pkey.store.password=%s' % command_options.keystore_pswd)
        if command_options.keyalias: flags.append('-Pkey.alias=%s' % command_options.keyalias)
        if command_options.keyalias_pswd: flags.append('-Pkey.alias.password=%s' % command_options.keyalias_pswd)

        # build the application
        build_in_dir( ".", flags )
    except BuildFailedException as e:
        print( e.message )
        exit(-1)

