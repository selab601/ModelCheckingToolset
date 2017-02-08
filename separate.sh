#!/bin/bash
usage(){
  echo "Usage: $0 [Rulelist] [AttributeList]" 1>&2
  exit 1
}

run_separater(){
  workdir=`dirname $0`
  cd $workdir
  $workdir/bin/VerifierGenerator $1 $2\
  && (cd $tmpdir; spin -a verifier.pml \
  && gcc pan.c \
  && rm -rf {pan.[bchmpt],verifier.pml}) \
  && $workdir/bin/StateManager $2 $workdir/tmp/InitialStates
}

. ./config

if [[ `make -q` -ne 0 ]] ; then
  echo "MAKE some files."
  make
fi

if [[ $# -ne 2 ]] ; then
  usage
fi

run_separater $1 $2