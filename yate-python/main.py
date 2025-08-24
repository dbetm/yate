import json
from pathlib import Path
import tkinter as tk
from datetime import datetime
from tkinter import filedialog, messagebox, font
from typing import List, Optional



class GUI:
    DEFAULT_WIDTH = 950
    DEFAULT_HEIGHT = 460

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("YATE")
        self.root.minsize(width=self.DEFAULT_WIDTH, height=self.DEFAULT_HEIGHT)
        self.root.attributes('-alpha', 0.9)
        # handle closing the window
        self.root.protocol("WM_DELETE_WINDOW", self.quit)

        default_font = font.Font(family="Comic Sans MS", size=14)
        self.content = tk.Text(
            self.root, wrap="word", padx=10, pady=10, font=default_font, spacing3=4
        )
        self.content.pack(expand=True, fill="both") # add to the root (parent)

        self.menubar = tk.Menu(self.root)
        self.filemenu = tk.Menu(self.menubar)
        self.emojimenu = tk.Menu(self.filemenu, tearoff=0)

    def run(self):
        self.root.mainloop()

    def quit(self):
        confirm_msg = "Are you sure you want to exit?"
        if self.content.edit_modified():
            if not messagebox.askokcancel("Changes unsaved", message=confirm_msg, icon="warning"):
                return

        self.root.quit()

    def confirm_open_new_file(self) -> bool:
        confirm_msg = "Are you sure you open another file without saving this?"
        if self.content.edit_modified():
            return messagebox.askokcancel("Changes unsaved - new file?", message=confirm_msg)

        return True

    def reset_modified(self):
        self.content.edit_modified(False)


class FileEditor:
    BEGIN_FILE = "0.0" # 0 row and 0 column
    ASSETS_PATH = Path("assets")
    METADATA_PATH = ASSETS_PATH / "metadata.json"
    EMOJIS_PATH = ASSETS_PATH / "emojis.txt"

    def __init__(self, gui: GUI):
        self.gui = gui
        self.metadata = self.__load_metadata()
        self.unsaved_changes = False
        self.filepath = None

        emojis = self.__load_emojis()
        self.__configure_menu(emojis)

    def __configure_menu(self, emojis: List[str]):
        self.gui.filemenu.add_command(label="New", command=self.new_file, accelerator="Ctrl+N")
        self.gui.filemenu.add_command(label="Open", command=self.open_file, accelerator="Ctrl+O")
        self.gui.filemenu.add_command(label="Save", command=self.save_file, accelerator="Ctrl+S")
        self.gui.filemenu.add_command(label="Save As", command=self.save_as, accelerator="Ctrl+w")
        self.gui.filemenu.add_separator()

        # Insert Emoji submenu
        for emoji in emojis:
            self.gui.emojimenu.add_command(label=emoji, command=lambda e = emoji: self.__insert_emoji(e))
        self.gui.filemenu.add_cascade(label="Insert emoji", menu=self.gui.emojimenu)

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

    def __load_metadata(self) -> dict:
        with open(self.METADATA_PATH, "r") as file:
            return json.load(file)

    def __load_emojis(self) -> List[str]:
        with open(self.EMOJIS_PATH, "r", encoding="utf-8") as f:
            return [line.strip() for line in f if line.strip()]

    def __insert_emoji(self, emoji: str):
        self.gui.content.insert(tk.INSERT, emoji)

    def __update_metadata(self, **kwargs) -> None:
        self.metadata.update(kwargs)

        with open(self.METADATA_PATH, "w", encoding="utf-8") as f:
            json.dump(self.metadata, f)

        print("Metadata was updated...")

    def __update_title(self):
        if self.filepath:
            filename = self.filepath.split("/")[-1]
        else:
            filename = "Untitled"
        self.gui.root.title(f"YATE - {filename}")

    def __solve_filepath_new_file(self, directory: str) -> Optional[str]:
        datetime_str = datetime.now().strftime("%Y%m%d-%H%M%S")
        return f"{directory}/untitled-{datetime_str}.txt"

    def new_file(self):
        self.filepath = None
        self.__update_title()
        # Delete all the current content
        self.gui.content.delete(self.BEGIN_FILE, tk.END)

    def save_file(self):
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        if not self.filepath:
            directory = filedialog.askdirectory(
                initialdir=self.metadata.get("last_path_used", "~/")
            )

            if not directory:
                return

            filepath = self.__solve_filepath_new_file(directory)
            self.filepath = filepath

        with open(self.filepath, "w") as f:
            f.write(text)

        self.__update_title()
        self.__update_metadata(last_path_used="/".join(self.filepath.split("/")[:-1]))
        self.gui.reset_modified()

    def save_as(self):
        # Get current text
        text = self.gui.content.get(self.BEGIN_FILE, tk.END)

        # Get dialog, the future request will remember the chosen directory
        filepath = filedialog.asksaveasfilename(defaultextension=".txt")

        if not filepath:
            return

        self.filepath = filepath

        try:
            with open(self.filepath, "w") as f:
                f.write(text)
        except:
            messagebox.showerror(title="Oops!", message="Unable to save file...")
        
        self.__update_title()
        self.__update_metadata(last_path_used="/".join(self.filepath.split("/")[:-1]))
        self.gui.reset_modified()

    def open_file(self):
        if not self.gui.confirm_open_new_file():
            return

        filepath = filedialog.askopenfilename(
            defaultextension=".txt", initialdir=self.metadata.get("last_path_used", "~/")
        )

        if not filepath:
            return

        self.filepath = filepath

        with open(self.filepath, "r") as f:
            content = f.read()

        self.__update_title()

        self.gui.content.delete(self.BEGIN_FILE, tk.END)
        self.gui.content.insert(self.BEGIN_FILE, content)

        self.__update_metadata(last_path_used="/".join(filepath.split("/")[:-1]))
        self.gui.reset_modified()



if __name__ == "__main__":
    gui = GUI()
    editor = FileEditor(gui)

    gui.run()
