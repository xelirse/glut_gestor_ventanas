#!/usr/bin/env python3

import subprocess
import random
import time
import sys

def get_windows():
    """
    Obtiene una lista de ventanas disponibles usando wmctrl.
    Retorna una lista de tuplas (ID de ventana, Título de ventana).
    """
    try:
        output = subprocess.check_output(["wmctrl", "-l"]).decode("utf-8")
        windows = []
        for line in output.splitlines():
            parts = line.split()
            if len(parts) >= 4:
                window_id = parts[0].strip()
                title = " ".join(parts[3:]).strip()
                windows.append((window_id, title))
        return windows
    except FileNotFoundError:
        print("Error: 'wmctrl' no encontrado. Por favor, instálalo (ej. sudo apt install wmctrl).")
        return []
    except subprocess.CalledProcessError as e:
        print(f"Error al ejecutar 'wmctrl -l': {e}.")
        print(f"Salida de error de wmctrl: {e.stderr.decode('utf-8') if e.stderr else 'N/A'}")
        return []
    except Exception as e:
        print(f"Error inesperado al obtener la lista de ventanas: {e}")
        return []

def get_window_geometry(window_id):
    """
    Obtiene las dimensiones (ancho, alto) y la posición (x, y) de una ventana específica usando xdotool.
    Retorna un diccionario con 'WIDTH', 'HEIGHT', 'X', 'Y'.
    """
    try:
        output = subprocess.check_output(["xdotool", "getwindowgeometry", "--shell", window_id]).decode("utf-8")
        geometry = {}
        for line in output.splitlines():
            if '=' in line:
                key, value = line.split('=', 1)
                if key in ['WIDTH', 'HEIGHT', 'X', 'Y']:
                    geometry[key] = int(value)
        return geometry
    except FileNotFoundError:
        print("Error: 'xdotool' no encontrado. Por favor, instálalo (ej. sudo apt install xdotool).")
        return None
    except subprocess.CalledProcessError as e:
        # Esto puede ocurrir si la ventana ya no existe o es inválida
        return None
    except Exception as e:
        print(f"Error al obtener la geometría de la ventana {window_id}: {e}")
        return None

def _activate_window(window_id):
    """
    Activa la ventana especificada para asegurar que esté en foco.
    """
    try:
        subprocess.run(["xdotool", "windowactivate", window_id], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(0.1) # Pequeña pausa para que el gestor de ventanas reaccione
    except subprocess.CalledProcessError as e:
        print(f"Advertencia: No se pudo activar la ventana {window_id}: {e.stderr.decode().strip()}")
    except FileNotFoundError:
        print("Error: 'xdotool' no encontrado. Por favor, instálalo.")
    except Exception as e:
        print(f"Error inesperado al activar la ventana {window_id}: {e}")

def _get_random_absolute_coords(window_id):
    """
    Calcula coordenadas absolutas aleatorias dentro de una ventana.
    Retorna (absolute_x, absolute_y) o (None, None) si falla.
    """
    geometry = get_window_geometry(window_id)
    if not geometry:
        return None, None

    width = geometry.get('WIDTH', 0)
    height = geometry.get('HEIGHT', 0)
    window_x = geometry.get('X', 0)
    window_y = geometry.get('Y', 0)

    if width <= 0 or height <= 0:
        return None, None

    random_x_relative_to_window = random.randint(0, width - 1)
    random_y_relative_to_window = random.randint(0, height - 1)

    absolute_x = window_x + random_x_relative_to_window
    absolute_y = window_y + random_y_relative_to_window
    return absolute_x, absolute_y

def perform_random_mouse_move(window_id):
    """
    Mueve el cursor del mouse visible a una posición aleatoria dentro de la ventana especificada.
    """
    absolute_x, absolute_y = _get_random_absolute_coords(window_id)
    if absolute_x is None:
        return False # Indicar que la acción no pudo realizarse

    _activate_window(window_id)
    
    try:
        print(f"Moviendo mouse a ({absolute_x}, {absolute_y}) en ventana {window_id}...")
        subprocess.run([
            "xdotool", "mousemove", str(absolute_x), str(absolute_y)
        ], check=True)
        return True
    except FileNotFoundError:
        print("Error: 'xdotool' no encontrado. Por favor, instálalo.")
    except Exception as e:
        print(f"Error al mover el mouse en {window_id}: {e}")
    return False

def perform_click(window_id, button):
    """
    Mueve el cursor a una posición aleatoria y realiza un click (1: izquierdo, 3: derecho).
    """
    absolute_x, absolute_y = _get_random_absolute_coords(window_id)
    if absolute_x is None:
        return False

    _activate_window(window_id)

    try:
        button_name = {1: "izquierdo", 3: "derecho"}.get(button, str(button))
        print(f"Moviendo mouse a ({absolute_x}, {absolute_y}) y haciendo click {button_name} en ventana {window_id}...")
        subprocess.run([
            "xdotool", "mousemove", str(absolute_x), str(absolute_y), "click", str(button)
        ], check=True)
        return True
    except FileNotFoundError:
        print("Error: 'xdotool' no encontrado. Por favor, instálalo.")
    except Exception as e:
        print(f"Error al hacer click {button_name} en {window_id}: {e}")
    return False

def perform_scroll(window_id):
    """
    Mueve el cursor a una posición aleatoria y realiza un scroll (4: arriba, 5: abajo).
    """
    absolute_x, absolute_y = _get_random_absolute_coords(window_id)
    if absolute_x is None:
        return False

    _activate_window(window_id)

    scroll_direction = random.choice([4, 5]) # 4 for scroll up, 5 for scroll down
    direction_name = "arriba" if scroll_direction == 4 else "abajo"

    try:
        print(f"Moviendo mouse a ({absolute_x}, {absolute_y}) y haciendo scroll {direction_name} en ventana {window_id}...")
        subprocess.run([
            "xdotool", "mousemove", str(absolute_x), str(absolute_y), "click", str(scroll_direction)
        ], check=True)
        return True
    except FileNotFoundError:
        print("Error: 'xdotool' no encontrado. Por favor, instálalo.")
    except Exception as e:
        print(f"Error al hacer scroll {direction_name} en {window_id}: {e}")
    return False

def main():
    print("Buscando ventanas disponibles...")
    windows = get_windows()

    if not windows:
        print("No se encontraron ventanas o hubo un error al listarlas. Asegúrate de que 'wmctrl' esté instalado y haya ventanas abiertas.")
        sys.exit(1)

    print("\nVentanas encontradas:")
    for i, (win_id, title) in enumerate(windows):
        print(f"{i+1}. ID: {win_id}, Título: '{title}'")

    selected_window_id = None
    selected_window_title = None

    while selected_window_id is None:
        try:
            choice = input("\nSelecciona el NÚMERO de la ventana en la que quieres actuar (o 'q' para salir): ").strip().lower()
            if choice == 'q':
                print("Saliendo del programa.")
                sys.exit(0)
            
            choice_idx = int(choice) - 1
            if 0 <= choice_idx < len(windows):
                selected_window_id, selected_window_title = windows[choice_idx]
                print(f"Has seleccionado: '{selected_window_title}' (ID: {selected_window_id})")
            else:
                print("Selección inválida. Por favor, introduce un número de la lista.")
        except ValueError:
            print("Entrada inválida. Por favor, introduce un número.")
        except KeyboardInterrupt:
            print("\nSelección de ventana cancelada. Saliendo del programa.")
            sys.exit(0)

    num_actions = 0
    while True:
        try:
            num_actions_input = input("\n¿Cuántas acciones aleatorias quieres que realice el mouse (movimientos, clicks, scroll)? (ej. 10, o 'q' para salir): ").strip().lower()
            if num_actions_input == 'q':
                print("Saliendo del programa.")
                sys.exit(0)
            
            num_actions = int(num_actions_input)
            if num_actions <= 0:
                print("Por favor, introduce un número positivo de acciones.")
                continue
            break
        except ValueError:
            print("Entrada inválida. Por favor, introduce un número entero o 'q'.")
        except KeyboardInterrupt:
            print("\nSelección de cantidad de acciones cancelada. Saliendo del programa.")
            sys.exit(0)

    print(f"\nIniciando {num_actions} acciones aleatorias en la ventana '{selected_window_title}'. Presiona Ctrl+C para detener.")

    # Lista de acciones posibles a elegir
    actions_list = ["move", "left_click", "right_click", "scroll"]

    try:
        for i in range(num_actions):
            # Seleccionar una acción aleatoria
            action_to_perform = random.choice(actions_list)

            print(f"\n[{i+1}/{num_actions}] Realizando acción '{action_to_perform}' en ventana: '{selected_window_title}' (ID: {selected_window_id})")

            # Realizar la acción y verificar si fue exitosa
            action_successful = False
            if action_to_perform == "move":
                action_successful = perform_random_mouse_move(selected_window_id)
            elif action_to_perform == "left_click":
                action_successful = perform_click(selected_window_id, button=1) # Botón 1 = click izquierdo
            elif action_to_perform == "right_click":
                action_successful = perform_click(selected_window_id, button=3) # Botón 3 = click derecho
            elif action_to_perform == "scroll":
                action_successful = perform_scroll(selected_window_id)
            
            if not action_successful:
                print(f"Advertencia: La acción '{action_to_perform}' no pudo realizarse en la ventana {selected_window_id}. Puede que la ventana haya sido cerrada o esté en un estado inválido. Terminando.")
                break # Salir del bucle si una acción falla
            
            # Pausa aleatoria entre 0.5 y 2.5 segundos antes de la siguiente acción
            time.sleep(random.uniform(0.5, 2.5))

        print(f"\nSe han completado las {i+1} acciones aleatorias en la ventana '{selected_window_title}'.")

    except KeyboardInterrupt:
        print("\nPrograma detenido por el usuario.")
    except Exception as e:
        print(f"\nUn error inesperado ocurrió en el bucle principal: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
