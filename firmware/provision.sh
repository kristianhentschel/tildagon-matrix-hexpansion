#!/bin/sh

DEFAULT_TARGET_MCU=CH32V006
DEFAULT_APP_NAME=lite_loop
DEFAULT_RETRIES=5
MINICHLINK=./ch32fun/minichlink/minichlink
DELAY=1

function usage() {
  echo "Matrix Hexpansion provisioning script"
  echo ""
  echo "Usage: ./provision.sh [options]"
  echo ""
  echo "Options:"
  echo "-h, --help       display this help and exit"
  echo "--bootloader     enable writing the bootloader program and option bytes (only required once)"
  echo "--target-mcu <mcu> override ch32fun TARGET_MCU variable (default = $DEFAULT_TARGET_MCU). Some programs will be too large to fit the CH32V003."
  echo "--retries <num>  number of retries for a failed step before bailing out. (default = $DEFAULT_RETRIES)"
  echo "--loop           once all steps for a board have succeeded, repeat the process for another next board (bulk provisioning)"
  echo "--interactive    require user confirmation before moving on to the next board if run in a loop"
  echo ""
  echo "--app <app>      name of the app folder to compile and flash (default=$DEFAULT_APP_NAME)"
  exit 0
}

function bail() {
  exit 1
}

# Default options
BOOTLOADER=0
TARGET_MCU=$DEFAULT_TARGET_MCU
RETRIES=$DEFAULT_RETRIES
INTERACTIVE=0
LOOP=0
APP_NAME=$DEFAULT_APP_NAME

export TARGET_MCU

# Parse CLI arguments
while [ $# -gt 0 ]; do
  case $1 in
    "-h")
      usage
      ;;
    "--help")
      usage
      ;;
    "--bootloader")
      BOOTLOADER=1
      ;;
    "--target-mcu")
      shift
      TARGET_MCU=$1
      ;;
    "--retries")
      shift
      RETRIES=$1
      ;;
    "--loop")
      LOOP=1
      ;;
    "--interactive")
      INTERACTIVE=1
      ;;
    "--app")
      shift
      APP_NAME=$1
      ;;
    *)
      echo "Unsupported argument $1"
      echo
      usage
      ;;
  esac
  shift
done

# Build minichlink if needed
if [ ! -e $MINICHLINK ]; then
  echo "Building minichlink"
  (cd `dirname $MINICHLINK` && make)
fi

# Build program
if [ ! -d $APP_NAME ]; then
  echo
  echo "Folder for program $APP_NAME not found"
  bail
fi
echo
echo "Building program $APP_NAME for $TARGET_MCU"
(cd ./$APP_NAME && make build) || bail

# Build bootloader, if needed
BOOTLOADER_DIR=
if [ $BOOTLOADER = 1 ]; then
  if [ $TARGET_MCU = "CH32V006" ]; then
    BOOTLOADER_DIR=bootloader_v006
  fi

  if [[ -n "$BOOTLOADER_DIR" && -d $BOOTLOADER_DIR ]]; then
    echo
    echo "Building bootloader for $TARGET_MCU from $BOOTLOADER_DIR"
    (cd $BOOTLOADER_DIR && make build) || bail
  else
    echo
    echo "No bootloader exists for $TARGET_MCU"
    bail
  fi
fi

# Loop over boards and steps
function confirm_if_interactive() {
  if [ $INTERACTIVE = 1 ]; then
    echo
    echo "Press RETURN to continue"
    read
  else
    sleep $DELAY
  fi
}

while
  # Flash bootloader
  if [ $BOOTLOADER = 1 ]; then
    echo
    echo "Flashing bootloader"
    SUCCESS=0
    i=1
    while [ $i -le $RETRIES ]; do
      $MINICHLINK -a -w $BOOTLOADER_DIR/bootloader.bin bootloader -B -D
      if [[ $? == 0 ]]; then
        echo
        echo "Bootloader written."
        SUCCESS=1
        break
      else
        echo "Attempt $i/$RETRIES failed"
        if [[ $i < $RETRIES ]]; then
          sleep $DELAY
        fi
      fi
      i=$((i+1))
    done
  else
    SUCCESS=1
  fi

  # Flash program, only attempt if previous step was successful (or skipped)
  if [ $SUCCESS = 1 ]; then
    echo
    echo "Flashing program"
    SUCCESS=0
    i=1
    while [ $i -le $RETRIES ]; do
      $MINICHLINK -a -w $APP_NAME/main.bin flash -D
      if [[ $? == 0 ]]; then
        echo
        echo "Program flashed."
        SUCCESS=1
        break
      else
        echo "Attempt $i/$RETRIES failed"
        if [[ $i < $RETRIES ]]; then
          sleep $DELAY
        fi
      fi
      i=$((i+1))
    done
  fi

  [ $LOOP -gt 0 ] || break
do (echo "\nNext board\n"; confirm_if_interactive); done
