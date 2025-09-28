# 🥤 Vending Machine con Semáforos (Productor–Consumidor)

Este proyecto implementa en **C con hilos y semáforos** la simulación de una **máquina expendedora (vending machine)**.  
El sistema coordina la interacción de **proveedores** que insertan productos en un buffer compartido, **consumidores** que los retiran, y un **facturador** que consolida estadísticas al final de la ejecución.

---

## ⚙️ Descripción del sistema

- **Buffer circular compartido**:  
  Espacio donde los proveedores insertan productos y los consumidores los extraen.  
  Controlado con **semáforos** para evitar condiciones de carrera.

- **Proveedores (`P`)**:  
  - Cada proveedor lee su lista de productos desde un fichero `proveedorX.dat`.  
  - Solo son válidos productos entre `'a'` y `'j'`.  
  - Los válidos se insertan en el buffer con su identificador de proveedor.  
  - Al terminar todos los proveedores, se introduce un producto especial `'z'` que señala el fin de la producción.

- **Consumidores (`C`)**:  
  - Cada consumidor extrae productos del buffer y guarda estadísticas en una **lista enlazada** (productos por tipo y por proveedor).  
  - Al detectar el producto `'z'`, entienden que no habrá más productos y finalizan su ejecución.  

- **Facturador**:  
  - Espera a que los consumidores terminen.  
  - Calcula:  
    - Total de productos consumidos.  
    - Distribución por proveedor.  
    - Cliente que más ha consumido.  
  - Escribe toda la información en el fichero de salida.  

---

## 🛠️ Tecnologías utilizadas
- Lenguaje: **C**  
- Librerías:  
  - `<pthread.h>` (hilos POSIX)  
  - `<semaphore.h>` (semáforos)  
  - Manejo de ficheros con `<stdio.h>`  

---

## 🚀 Compilación y ejecución

### Compilación
```bash
gcc bendingmachine.c -o vending -lpthread
