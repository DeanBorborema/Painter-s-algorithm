# Algoritmo do Pintor em OpenGL (C)

Este projeto implementa uma cena 3D simples utilizando **OpenGL/GLUT** em C, demonstrando:

- **Visualização 3D** (câmera com perspectiva e interação).
- **Iluminação e sombreamento** (GL_LIGHT0 + Gouraud shading).
- **Mapeamento de textura** (xadrez procedural em um cubo).
- **Curva paramétrica** (Bezier cúbica em 3D).
- **Algoritmo do Pintor (Painter's Algorithm)** para visibilidade, com opção de alternar para **z-buffer**.

---

## 📖 Sobre o Algoritmo do Pintor

O **Algoritmo do Pintor** resolve o problema da **visibilidade** em computação gráfica.  
A ideia é desenhar os objetos **do mais distante para o mais próximo da câmera**, de modo que os mais próximos "cobrem" os de trás, como se um pintor estivesse pintando uma tela em camadas.

Este método é simples, mas apresenta limitações em casos de objetos que se **interceptam ou se entrelaçam**, onde seria necessário **dividir polígonos** para resolver ambiguidades.  
Hoje em dia, o z-buffer é mais usado, mas o Algoritmo do Pintor é importante como base conceitual.

---

## ⚙️ Como o código funciona

1. Um **cubo texturizado** é desenhado. Antes de cada frame:
   - Calcula-se a **profundidade média de cada face** no espaço da câmera.
   - Ordena-se as faces de trás para frente com `qsort`.
   - Renderiza-se na ordem (sem usar `GL_DEPTH_TEST`).

2. Um **"chão"** é desenhado como um cubo achatado, também ordenado pelo algoritmo.

3. Uma **curva de Bezier cúbica** é renderizada em 3D para demonstrar geometria paramétrica.

4. O usuário pode interagir com o teclado para mover a câmera e alternar entre **painter** e **z-buffer**.

---

## ⌨️ Controles

- `w/s` → inclinar para cima/baixo  
- `z/x` → girar esquerda/direita  
- `a/d` → aproximar/afastar câmera  
- `t` → alternar **Painter (OFF depth-test)** ↔ **Z-buffer (ON)**  
- `ESC` → sair  

---

## 🖥️ Compilação

### Dependências

- **Linux**: `freeglut3-dev`, `mesa-utils`  
- **Windows (MSYS2/MinGW)**: `freeglut`  
- **macOS**: `freeglut` via Homebrew  

---

## 🛠️ Compilação com Makefile

Este projeto já possui um **Makefile** configurado para Linux, macOS e Windows (MSYS2/MinGW).

### Compilar
```bash
make
