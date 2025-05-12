export PYTHONPATH="../python"
numfiles=$(ls -l simulation/users | grep -v ^d | wc -l)
cd build

if [ $1 ] && (( $1==-1 ));
then
    echo "true"
else
    echo "false"
fi

exit

./coin_selection_c "$@"
cd ..
