import tkinter as tk
from datetime import datetime
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

    def quit(self):
        self.root.quit()


class FileEditor:
    BEGIN_FILE = "0.0" # 0 row and 0 column

    def __init__(self, gui: GUI):
        self.filename = "Untitled"
        self.gui = gui

        self.__configure_menu()

    def __configure_menu(self):
        self.gui.filemenu.add_command(label="New", command=self.new_file, accelerator="Ctrl+N")
        self.gui.filemenu.add_command(label="Open", command=self.open_file, accelerator="Ctrl+O")
        self.gui.filemenu.add_command(label="Save", command=self.save_file, accelerator="Ctrl+S")
        self.gui.filemenu.add_command(label="Save As", command=self.save_as, accelerator="Ctrl+w")
        self.gui.filemenu.add_separator()
        self.gui.filemenu.add_command(label="Quit", command=self.gui.quit, accelerator="Ctrl+Q")

        self.gui.menubar.add_cascade(label="File", menu=self.gui.filemenu)

        self.gui.root.config(menu=self.gui.menubar)

        # Bind shortcuts
        self.gui.root.bind_all("<Control-n>", lambda _: self.new_file())
        self.gui.root.bind_all("<Control-o>", lambda _: self.open_file())
        self.gui.root.bind_all("<Control-s>", lambda _: self.save_file())
        self.gui.root.bind_all("<Control-w>", lambda _: self.save_as())
        self.gui.root.bind_all("<Control-q>", lambda _: self.gui.quit())

    def __update_title(self, filename: Optional[str] = None, filepath: Optional[str] = None):
        assert filename or filepath

        self.filename = filename if filename else filepath.split("/")[-1]
        self.gui.root.title(f"YATE - {self.filename}")

    def __solve_filepath_new_file(self, directory: str) -> Optional[str]:
        datetime_str = datetime.now().strftime("%Y%m%d-%H%M%S")
        return f"{directory}/{self.filename}-{datetime_str}.txt"

    def new_file(self):
        self.__update_title(filename=self.filename)
        # Delete all the current content
        self.gui.content.delete(self.BEGIN_FILE, tk.END)

    def save_file(self):
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        directory = filedialog.askdirectory(initialdir="~/")

        if not directory:
            return

        filepath = self.__solve_filepath_new_file(directory)

        with open(filepath, "w") as f:
            f.write(text)

        self.__update_title(filepath=filepath)

    def save_as(self):
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        # Get dialog
        filepath = filedialog.asksaveasfilename(defaultextension=".txt")

        if not filepath:
            return

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

        self.gui.content.delete(self.BEGIN_FILE, tk.END)
        self.gui.content.insert(self.BEGIN_FILE, content)



if __name__ == "__main__":
    gui = GUI()
    editor = FileEditor(gui)

    gui.run()
