from cgi import print_form
from pydoc import stripid
import socket
import time
import tkinter as tk
from tkinter import simpledialog, messagebox
import select
import threading
import sys
import re


class RPSClient:

    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.client_socket = None
        self.player_name = ""
        self.disconnected_because_timeout = False

        # Initialize GUI

        self.root = tk.Tk()
        self.root.geometry("400x300");
        self.root.title("Rock Paper Scissors Game")        

        # contains your of opponent
        self.general_label = tk.Label(self.root, text="Welcome to Rock Paper Scissors!")        
        self.general_label.config(font=('Helvatical bold',15), text="")
        self.general_label.pack()
        
        # contains your of opponent
        self.name_label = tk.Label(self.root, text="")
        self.name_label.pack()
        
        # label for your choice
        self.your_label = tk.Label(self.root, text="")
        self.your_label.pack()        
        
        # label for opponent choice
        self.opponent_label = tk.Label(self.root, text="")
        self.opponent_label.pack()
        
        self.result_label = tk.Label(self.root, text="")
        self.result_label.config(font=('Helvatical bold',20), text="")
        self.result_label.pack()

        # describes what to do
        self.status_label = tk.Label(self.root, text="Enter your name")
        self.status_label.pack()        

        # Connection label
        self.connection_label = tk.Label(self.root, text="")
        self.connection_label.pack()
        
        # Create game buttons
        self.create_game_buttons()
        self.disable_game_buttons()
        
        # create and hide decision buttons
        self.create_decision_buttons()
        self.hide_decision_buttons()

        # Ask for the player's name
        self.ask_player_name()

        # Run the GUI loop
        self.root.mainloop()
         

    def create_game_buttons(self):
        game_frame = tk.Frame(self.root)
        game_frame.pack()
        self.rock_button = tk.Button(game_frame, text="Rock", command=lambda: self.select_choice('rock'))
        self.rock_button.pack(side=tk.LEFT)
        self.paper_button = tk.Button(game_frame, text="Paper", command=lambda: self.select_choice('paper'))
        self.paper_button.pack(side=tk.LEFT)
        
        self.scissors_button = tk.Button(game_frame, text="Scissors", command=lambda: self.select_choice('scissors'))
        self.scissors_button.pack(side=tk.LEFT)

    def create_decision_buttons(self):
        decission_frame = tk.Frame(self.root)
        decission_frame.pack()
        self.yes_button = tk.Button(decission_frame, text="Yes", command=lambda: self.select_decission('again'), bg='green')
        self.yes_button.pack(side=tk.LEFT)
        self.no_button = tk.Button(decission_frame, text="No", command=lambda: self.select_decission('bye'), bg='red')
        self.no_button.pack(side=tk.LEFT)
        
    def disable_game_buttons(self):
        self.rock_button['state'] = tk.DISABLED
        self.paper_button['state'] = tk.DISABLED
        self.scissors_button['state'] = tk.DISABLED

    def enable_game_buttons(self):
        print("Enabling buttons...")  

        self.rock_button['state'] = tk.NORMAL
        self.paper_button['state'] = tk.NORMAL
        self.scissors_button['state'] = tk.NORMAL 
        
    def hide_game_buttons(self):
        self.rock_button.pack_forget()
        self.paper_button.pack_forget()
        self.scissors_button.pack_forget()
    
    def show_game_buttons(self):
        
        self.rock_button.pack(side=tk.LEFT)
        self.paper_button.pack(side=tk.LEFT)
        self.scissors_button.pack(side=tk.LEFT)        

    def hide_decision_buttons(self):
        self.yes_button.pack_forget()
        self.no_button.pack_forget()
    
    def show_decision_buttons(self):
        self.yes_button.pack(side=tk.LEFT)
        self.no_button.pack(side=tk.LEFT)
        
    def set_initial_text(self):
        self.your_label.configure(text = "")
        self.opponent_label.configure(text = "")
        self.result_label.configure(text = "")        
        self.connection_label.configure(text = "")
        self.name_label.configure(text="")
        
    def select_choice(self, choice):
        print("in select choice")
        self.your_label.configure(text = f"You chose: {choice}")        
        self.status_label.configure(text = f"Wait for opponent's move'")
        self.send_data(choice)
        self.disable_game_buttons
    
    def select_decission(self, decission):
        if decission == "bye":
            # end game and close window
            self.say_goodbye()            
            self.root.destroy()
        else:
            self.hide_decision_buttons()
            self.show_game_buttons()
            self.disable_game_buttons()
            self.set_initial_text()
            self.start_game()
            self.send_data(decission)

    def ask_player_name(self, first_time = True):
        print("ask for name")
        if first_time:
            hint = ""
        else:
            hint = " without \" and \\ and :"
        self.player_name = simpledialog.askstring("Name", f"Please tell us your name{hint}", parent=self.root)
        if self.player_name:
            if self.player_name.__contains__("\""):
                self.ask_player_name(False)
                return
            elif self.player_name.__contains__("\\"):
                self.ask_player_name(False)
                return
            elif self.player_name.__contains__(":"):
                self.ask_player_name(False)
                return
            else:
                self.start_game()
        else:
            self.player_name = "Player"
        self.connect_to_server()
            
    def start_game(self):
        self.general_label.config(text=f"Hello {self.player_name}")  
        self.status_label.config(text=f"Wait for an opponent...")  
        
    def say_goodbye(self): 
        try:
            self.client_socket.sendall(("bye").encode())
        except Exception as e:
            print("connect_to_server catch")
            # ignore

    def connect_to_server(self):
        try:
            print("Connecting to server...")
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.connect((self.host, self.port))
            print("Connected to server.")
            if self.player_name:
                prefixed_name = "name:" + self.player_name.strip()
                self.client_socket.sendall(prefixed_name.encode())
                threading.Thread(target=self.listen_to_server, daemon=True).start()

        except Exception as e:
            print("connect_to_server catch")
            self.connection_label.config(text="connection failed")            
            self.connection_label.update()
            self.reconnect_to_server();


    def send_data(self, data):
        try:
            if self.client_socket is None:
                raise ValueError("Connection to server is not active.")
             # Send the data to the server

            self.client_socket.sendall((data).encode())
            self.disable_game_buttons()
 
            print(f"data sent - {data}")

        except socket.error as e:
            messagebox.showerror("Socket Error", f"Socket error occurred: {e}")
            self.reconnect_to_server()

        except ValueError as ve:
            if self.disconnected_because_timeout is False:
                messagebox.showerror("Error", str(ve))
            self.reconnect_to_server()

        except Exception as e:
            messagebox.showerror("Error", f"An unexpected error occurred: {e}")

    def reconnect_to_server(self):        
        print("reconnect_to_server")
        for attempt in range(3):  # Try to reconnect a few times
            try:
                print(f"Reconection attempt #{attempt+1}/3 in progress")
                self.connection_label.config(text=f"Reconection attempt #{attempt+1}/3 in progress")
                self.connection_label.update()
                self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.client_socket.connect((self.host, self.port))
                self.client_socket.sendall(self.player_name.strip().encode())
                threading.Thread(target=self.listen_to_server, daemon=True).start()
                # messagebox.showinfo("Reconnection", "Reconnected to the server.")
                self.connection_label.config(text=f"Reconnected to the server.")
                self.connection_label.update()
                time.sleep(1)  # Wait a bit before trying to reconnect again                
                self.connection_label.config(text="")
                self.connection_label.update()
                print("connected")
                return

            except socket.error as e:
                print("socket error")
                if attempt < 2:                     
                    for seconds in range(5): # for loop for countdown seconds to next attempt                           
                        self.connection_label.config(text=f"Next reconnection attempt in {5-seconds} s")
                        self.connection_label.update()
                        time.sleep(1)  # Wait a bit before trying to reconnect again
            
            except Exception as e:
                print("other error")
                if attempt < 2:      
                    for seconds in range(5): # for loop for countdown seconds to next attempt                           
                        self.connection_label.config(text=f"Next reconnection attempt in {5-seconds} s")                        
                        self.connection_label.update()
                        time.sleep(1)  # Wait a bit before trying to reconnect again

        messagebox.showerror("Reconnection Failed", "Could not connect to the server.")
        self.root.destroy()  # Close the GUI if reconnection fails       



    def handle_connection_error(self, error, destroy):

        # Handle connection errors, show message box, and possibly retry or close the game
        print(f"Failed to connect to server: {error}")
        messagebox.showerror("Connection Error", f"Failed to connect to server: {error}")
        # Decide whether to destroy the GUI or attempt reconnection
        if destroy: 
            self.root.destroy() 
    
    def display_error_and_close(self, error_message):
        messagebox.showerror("Error", error_message)
        self.safe_close_gui()

    def safe_close_gui(self):
        if self.root:
            self.root.destroy()
            self.root = None

    def handle_opponent_disconnection(self):
        self.general_label.config(text="Your opponent has been disconnected.")

    def handle_opponent_issue_notification(self):
        # Update the GUI to notify the player about the opponent's potential connection issues
        self.connection_label.config(text="Your opponent may be having connection issues.")
        self.connection_label.update()
    
    def listen_to_server(self):
        while True:
            try:
                message = self.client_socket.recv(1024).decode()
                if not message:
                    # If the server closes the connection, recv will return an empty string.
                    print("Server closed the connection.")
                    self.root.after(0, self.safe_close_gui)
                    break

                print(f"Raw received message: {message}")

                # Split the message by semicolons and filter out empty parts
                parts = [part for part in message.split(';') if part.strip()]

                # Now, build the item dictionary only with parts containing a colon
                item = {}
                for part in parts:
                    if ':' in part:
                        key, value = part.split(':', 1)
                        item[key.strip()] = value.strip()

                if "ping" in message:
                    self.send_data("pong")

                if 'name' in item:
                    name_message = item['name']
                    if name_message.startswith("cut"):
                        cut_name = name_message.split(" ", 1)[1]  # Get the cut name after the "cut " keyword
                        self.connection_label.config(text=f"Your name is too long. It was cut to {cut_name}")
                        self.connection_label.update()
                        



                # Handle commands if present
                if 'command' in item and item['command'] == 'make_move':
                    self.status_label.configure(text=item.get('message', ''))
                    self.enable_game_buttons()

                if 'success' in item:
                    # Handle success message
                    print(item['success'])
                    # You can update the GUI or initiate the next step of your game here
        
                if 'error' in item:
                    error_message = item['error']
                    print(item['error'])
                    # Use lambda to ensure that display_error_and_close is called from the GUI thread
                    self.root.after(0, lambda: self.display_error_and_close(error_message))
                    break

                if 'opponent_name' in item and item['opponent_name']:
                    self.name_label.config(text=f"Your opponent is {item['opponent_name']}")
                    self.status_label.configure(text="Please make your move.")
                    self.enable_game_buttons()

                if 'choice' in item and item['choice']:
                    self.your_label.config(text=f"You chose {item['choice']}")
                    self.disable_game_buttons()

                if 'opponent_choice' in item and item['opponent_choice']:
                    self.opponent_label.config(text=f"Opponent chose {item['opponent_choice']}")
                    self.status_label.configure(text="")

                if 'is_winner' in item and item['is_winner'] != "-1":
                    result = int(item['is_winner'])
                    if result == 1:
                        self.result_label.config(text="You won!")
                    elif result == 0:
                        self.result_label.config(text="Draw!")
                    else:
                        self.result_label.config(text="You lost!")
                    self.status_label.configure(text="Do you want to play again?")
                    self.hide_game_buttons()
                    self.show_decision_buttons()

                # After parsing the message into the item dictionary
                if 'info' in item:
                    # This will handle all 'info' messages
                    info_message = item['info']
                    self.connection_label.config(text=info_message)
                    self.connection_label.update()
                    # Check if the info message is about the opponent having problems
                    if "Opponent issues" in info_message:
                        self.root.after(0, self.handle_opponent_issue_notification)

                    elif "timeout" in info_message:
                        self.disconnected_because_timeout = True
                        self.client_socket = None
                        self.root.after(0, lambda: self.handle_opponent_disconnection())
                        break  # Stop listening if disconnected due to timeout.
                    elif "Disconnecting" in info_message:
                        self.root.after(0, self.handle_opponent_disconnection)
                        break  # Stop listening for messages if disconnecting.
                    elif "remaining" in info_message:
                        # Extract the number of seconds remaining from the message
                        match = re.search(r"remaining (\d+) seconds", info_message)
                        if match:
                            timeout_seconds = match.group(1)  # This is the 'X' seconds extracted from the message
                            self.connection_label.config(text=f"You have {timeout_seconds} seconds to answer.")
                            self.connection_label.update()


            except Exception as e:
                error_message = f"An error occurred while receiving"
                # Schedule the custom method to run on the main thread
                self.root.after(0, lambda: self.display_error_and_close(error_message))
                print(error_message)
                break




if __name__ == "__main__":
    host = 'localhost'  # Default host
    port = 50000        # Default port

    if len(sys.argv) == 3:
        host = sys.argv[1]
        port = int(sys.argv[2])

    client = RPSClient(host, port)
