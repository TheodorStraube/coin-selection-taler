export PYTHONPATH="../python"
numfiles=$(ls -l simulation/users | grep -v ^d | wc -l)
cd build

./coin_selection_c "$@"
cd ..

if [ $1 ] && (( $1==-1 ));
then
    echo "true"
else
    echo "false"
fi
