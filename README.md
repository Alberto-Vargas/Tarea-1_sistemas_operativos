# Planificador Dieciochero Tarea 1 sistemas Operativos



Integrante Alberto Vargas



En la actividad se ejecutan procesos independientes "fork" respetando dependencias y limites de concurrencia K los procesos se comunican mediante

pipes. 





## Compilación 

se uso" c++ y se utilizo lo siguiente entregado por el profesor: 

g++ -Wall -Wextra -std=c++17 planificador.cpp -o planificador -lpthread

./planificador plan.txt K "



## Funciones implementadas



\- leer\_plan: lee plan.txt y guarda las actividades.

\- construir\_grafo: conecta las actividades según sus dependencias.

\- lanzar: crea los pipes y el proceso hijo con fork.

\- proceso\_hijo: lee los insumos, espera el tiempo y avisa al padre.

\- abortar\_rama: cancela las actividades que dependían de una que falló.

\- cancelar\_todo: mata a los hijos cuando se presiona Ctrl+C.



## decisión de diseño 

\- Procesos con fork ya que el enunciado nos dice que no se puede usar hilos

\- waitpid en vez de un bucle, para no gastar CPU esperando.

\- Solo se lanza un proceso si hay menos de K vivos.

\- Dos pipes por actividad: uno para darle insumos y otro para recibir su resultado.

\- Si una actividad falla, solo se aborta su rama.

\- Ctrl+C activa una bandera y el padre termina a los hijos ordenadamente.

