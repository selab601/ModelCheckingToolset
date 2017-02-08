#!/bin/bash

# exec 1> >(tee -a ./log/`date '+%F'`.log)
# exec 2> >(tee -a ./log/error.log >&2)

echo(){ builtin echo "[`basename $0`]:" $@; }

usage(){
  echo "Usage: "`basename $0`" [RulesDirPath]" 1>&2
  exit 1
}

confirm() {
  read -p "`echo $1`(y/n)?" answer 2>/dev/tty
  case $answer in
    [yY])
      return 0;;
    [nN])
      return 1;;
    * ) 
      confirm "$1";
  esac
}

run_statemanager(){
  workdir=`dirname $0`
  targetDir=$1/ruleCuts_result_merged
  cd $workdir
  for i in `seq 0 $(expr \\( $CORENUM \\* 2 \\) - 1 )`
  do
    relPath="${relPath} --relationships:Rule ${targetDir}/rules-header.csv,${targetDir}/rules${i}.csv"
  done
  echo "Generating DB..."\
  && neo4j-import \
     --into $NEO4J_DATABASE_PATH \
     --nodes:A $targetDir/StatesHeader_A,$targetDir/InitialStates_A \
     --nodes:B $targetDir/StatesHeader_B,$targetDir/InitialStates_B \
     $relPath\
  && neo4j restart \
  && echo "Done."
}

echo `date`

. ./config
make -q
is_updated=$?

if [[ $# -ne 1 ]] ; then
  usage
fi

if [[ -e $NEO4J_DATABASE_PATH ]] ; then
  echo "\"$NEO4J_DATABASE_PATH\" already exist."
  confirm "Rename \"$NEO4J_DATABASE_PATH\" ?"
  if [ $? -eq 0 ]; then
    NEO4J_DATABASE_PATH_TMP=$NEO4J_DATABASE_PATH
    while [ -e $NEO4J_DATABASE_PATH_TMP ]
    do
      NEO4J_DATABASE_PATH_TMP="$NEO4J_DATABASE_PATH_TMP""_tmp"
    done
    echo "Old: "$NEO4J_DATABASE_PATH_TMP
    mv $NEO4J_DATABASE_PATH $NEO4J_DATABASE_PATH_TMP
  else
    echo "Cancelled."
    exit 1
  fi
fi

if [[ $is_updated -eq 1 ]] ; then
  echo "MAKE"
  make | sed "s/^/[make]/g"
fi

run_statemanager $1