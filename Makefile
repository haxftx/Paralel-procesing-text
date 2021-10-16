build:
	mpicc paralel_procesing.c -Wall -Wextra -lpthread -o main
run:
	mpirun -np 5 ./main $I 
clear:
	rm main
