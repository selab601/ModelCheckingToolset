#!/bin/bash

# exec 1> >(tee -a ./log/`date '+%F'`.log)
# exec 2> >(tee -a ./log/error.log >&2)


echo(){ builtin echo "[`basename $0`]:" $@; }

usage(){
  echo "Usage: "`basename $0`" [Rulelist] [AttributesTable]" 1>&2
  exit 1
}

run_statemanager(){
  workdir=`dirname $0`
  cd $workdir
  tmpdir=`mktemp -d -t tmp`
  resultdir=$(dirname $1)/$(basename $1)_result
  mkdir $resultdir
  for i in `seq $CORENUM`
  do
    relPath="${relPath} --relationships:Rule ${tmpdir}/rules-header.csv,${tmpdir}/rules`expr $i - 1`.csv"
  done
  echo "Generating initialstates..." && $workdir/bin/VerifierGenerator $1 $2 $tmpdir\
  && echo "Generating verifier..." && (cd $tmpdir; spin -a verifier.pml && gcc pan.c)\
  && echo "Running StateManager..." && $workdir/bin/StateManager $2 $tmpdir/InitialStates $tmpdir $resultdir\
  && echo "Moving Files..." && mv $tmpdir/InitialStates $resultdir/ \
  && echo "Done."
}

echo `date`

. ./config
make -q
is_updated=$?

if [[ $# -ne 2 ]] ; then
  usage
fi

if [[ $is_updated -eq 1 ]] ; then
  echo "MAKE"
  make | sed "s/^/[make]/g"
fi

run_statemanager $1 $2