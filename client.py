import socket
import tkinter as tk
from tkinter import simpledialog, messagebox
import select
import threading


class RPSClient:


    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.client_socket = None
        self.player_name = ""

        # Initialize GUI

        self.root = tk.Tk()
        self.root.title("Rock Paper Scissors Game")
        self.status_label = tk.Label(self.root, text="Welcome to Rock Paper Scissors!")
        self.status_label.pack()

        # Opponent and result labels

        self.opponent_label = tk.Label(self.root, text="Opponent: ")
        self.opponent_label.pack()
        self.result_label = tk.Label(self.root, text="")
        self.result_label.pack()

        # Create game buttons
        self.create_game_buttons()

        # Ask for the player's name
        self.ask_player_name()

        # Run the GUI loop
        self.root.mainloop()



    def ask_player_name(self):
        self.player_name = simpledialog.askstring("Name", "Please tell us your name", parent=self.root)
        if self.player_name:
            self.status_label.config(text=f"Hello {self.player_name}, waiting for an opponent...")
            self.connect_to_server()
        else:
            self.player_name = "Player"
            self.connect_to_server()


    def create_game_buttons(self):
        tk.Label(self.root, text="Choose:").pack()
        frame = tk.Frame(self.root)
        frame.pack()
        self.rock_button = tk.Button(frame, text="Rock", command=lambda: self.send_choice('rock'))
        self.rock_button.pack(side=tk.LEFT)
        self.paper_button = tk.Button(frame, text="Paper", command=lambda: self.send_choice('paper'))
        self.paper_button.pack(side=tk.LEFT)
        self.scissors_button = tk.Button(frame, text="Scissors", command=lambda: self.send_choice('scissors'))
        self.scissors_button.pack(side=tk.LEFT)


    def connect_to_server(self):
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.connect((self.host, self.port))
            self.client_socket.sendall(self.player_name.encode() + b'\n')
            threading.Thread(target=self.listen_to_server, daemon=True).start()

        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect to server: {e}")
            self.root.destroy()


    def wait_for_match(self):
        while True:
            message = self.client_socket.recv(1024).decode()
            if "Match found with" in message:
                opponent_name = message.split('with')[1].split('!')[0].strip()
                self.status_label.config(text=f"Your opponent {opponent_name} has arrived. You have 59 seconds to submit your move.")
                self.root.after(0, self.enable_buttons)
                # Now, also wait for the result after sending the move
                self.root.after(0, self.listen_for_result)  
                break

    def listen_for_result(self):
        try:
            # Listen for the result immediately, as the server should send the result once both players have made their moves
            result = self.client_socket.recv(1024).decode()
            if result:
                self.process_result(result)
            else:
                messagebox.showerror("Error", "No result received from server.")
                self.enable_buttons()
        except Exception as e:
            messagebox.showerror("Error", f"An error occurred while receiving: {e}")                


    def send_choice(self, choice):
        try:
            if self.client_socket is None:
                raise ValueError("Connection to server is not active.")
             # Send the choice to the server

            self.client_socket.sendall((choice + '\n').encode())
            self.disable_buttons()
 
            # Now that the choice has been sent, listen for the result
            self.listen_for_result()

        except socket.error as e:
            messagebox.showerror("Socket Error", f"Socket error occurred: {e}")
            self.reconnect_to_server()

        except ValueError as ve:
            messagebox.showerror("Error", str(ve))
            self.reconnect_to_server()

        except Exception as e:
            messagebox.showerror("Error", f"An unexpected error occurred: {e}")
            self.reconnect_to_server()



    def disable_buttons(self):
        self.rock_button['state'] = tk.DISABLED
        self.paper_button['state'] = tk.DISABLED
        self.scissors_button['state'] = tk.DISABLED



    def enable_buttons(self):
        print("Enabling buttons...")  # Debug print

        self.rock_button['state'] = tk.NORMAL
        self.paper_button['state'] = tk.NORMAL
        self.scissors_button['state'] = tk.NORMAL 


    def receive_result(self):
        try:
            result = self.client_socket.recv(1024).decode()
            if result:
                self.process_result(result)  # This is a new method you should define
            else:

                messagebox.showerror("Error", "No result received from server.")
                self.enable_buttons()
        except Exception as e:

            messagebox.showerror("Error", f"An error occurred while receiving: {e}")


    def reconnect_to_server(self):
        for attempt in range(3):  # Try to reconnect a few times
            try:
                self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.client_socket.connect((self.host, self.port))
                self.client_socket.sendall(self.player_name.encode() + b'\n')
                messagebox.showinfo("Reconnection", "Reconnected to the server.")
                return

            except socket.error as e:
                time.sleep(2)  # Wait a bit before trying to reconnect again
                continue

        messagebox.showerror("Reconnection Failed", "Could not reconnect to the server.")
        self.root.destroy()  # Close the GUI if reconnection fails       



    def handle_connection_error(self, error):

        # Handle connection errors, show message box, and possibly retry or close the game
        print(f"Failed to connect to server: {error}")
        messagebox.showerror("Connection Error", f"Failed to connect to server: {error}")
        # Decide whether to destroy the GUI or attempt reconnection
        # self.root.destroy() 


    def process_result(self, result):
        # This method processes the result received from the server
        parts = result.split('\n')
        if len(parts) >= 2:
            opponent_name, game_result = parts[:2]
            self.opponent_label.config(text=f"Opponent: {opponent_name}")
            self.result_label.config(text=game_result)
        else:
            self.result_label.config(text=result)
        self.enable_buttons() 


    def listen_to_server(self):
        while True:
            try:
                message = self.client_socket.recv(1024).decode()
                if "Match found with" in message:

                    opponent_name = message.split('with')[1].split('!')[0].strip()

                    self.status_label.config(text=f"Your opponent {opponent_name} has arrived. Please make your move.")
                    self.enable_buttons()
                elif "wins!" in message or "It's a draw!" in message:
                    self.process_result(message)
                    self.disable_buttons()  # Assuming you want to disable buttons after the game is over
                    break  # Break if you want to end the listening after receiving the result
            except Exception as e:
                messagebox.showerror("Error", f"An error occurred while receiving: {e}")







                break























if __name__ == "__main__":


    client = RPSClient('localhost', 50000)



















