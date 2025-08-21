import tkinter as tk
from tkinter import filedialog, messagebox
from typing import Optional



class GUI:
    DEFAULT_WIDTH = 600
    DEFAULT_HEIGHT = 600

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("YATE")
        self.root.minsize(width=self.DEFAULT_WIDTH, height=self.DEFAULT_HEIGHT)
        self.root.maxsize(width=self.DEFAULT_WIDTH, height=self.DEFAULT_HEIGHT)

        self.content = tk.Text(self.root, wrap="word")
        self.content.pack(expand=True, fill="both") # add to the root (parent)

        self.menubar = tk.Menu(self.root)
        self.filemenu = tk.Menu(self.menubar)

    def run(self):
        self.root.mainloop()


class FileEditor:
    def __init__(self, gui: GUI):
        self.filename = "Untitled"
        self.gui = gui

        self.__configure_menu()

    def __configure_menu(self):
        self.gui.filemenu.add_command(label="New", command=self.new_file)
        self.gui.filemenu.add_command(label="Open", command=self.open_file)
        self.gui.filemenu.add_command(label="Save", command=self.save_file)
        self.gui.filemenu.add_command(label="Save As", command=self.save_as)
        self.gui.filemenu.add_separator()
        self.gui.filemenu.add_command(label="Quit", command=self.gui.root.quit)

        self.gui.menubar.add_cascade(label="File", menu=self.gui.filemenu)

        self.gui.root.config(menu=self.gui.menubar)

    def __update_title(self, filename: Optional[str] = None, filepath: Optional[str] = None):
        assert filename or filepath

        self.filename = filename if filename else filepath.split("/")[-1]
        self.gui.root.title(f"YATE - {self.filename}")

    def new_file(self):
        self.__update_title(filename=self.filename)
        # Delete all the current content
        self.gui.content.delete("0.0", tk.END)

    def save_file(self):
        # Get current text
        text = self.gui.content.get("0.0", tk.END)

        with open(f"{self.filename}.txt", "w") as f:
            f.write(text)

    def save_as(self):
        # Get current text
        text = self.gui.content.get("0.0", tk.END)

        # Get dialog
        filepath = filedialog.asksaveasfilename(defaultextension=".txt")

        try:
            with open(filepath, "w") as f:
                f.write(text)
        except:
            messagebox.showerror(title="Oops!", message="Unable to save file...")
        
        self.__update_title(filepath=filepath)


    def open_file(self):
        filepath = filedialog.askopenfilename(defaultextension=".txt")

        with open(filepath, "r") as f:
            content = f.read()

        self.__update_title(filepath=filepath)

        self.gui.content.delete("0.0", tk.END)
        self.gui.content.insert("0.0", content)



if __name__ == "__main__":
    gui = GUI()
    editor = FileEditor(gui)

    gui.run()
