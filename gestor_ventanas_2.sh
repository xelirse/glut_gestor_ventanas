#!/bin/sh

n=gestor_ventanas_2
rm ./$n
g++ $n.cpp -o $n -lXcomposite -lXrender -lglut -lGL -lGLU -lX11 -lXext
if [[ -f ./$n ]];then
	cp -vf ./$n /bin
	$n
fi
