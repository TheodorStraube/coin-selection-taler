from dataclasses import dataclass
from os import listdir
from typing import Generator
from dataclass_wizard import JSONWizard
from tqdm import tqdm
import json
from pathlib import Path

PATH = Path("../coin-selection-c-master/simulation/results")

@dataclass
class Coin:
    value: int

@dataclass
class DataPoint(JSONWizard):
    time: int
    coins: list[Coin]

    def __repr__(self):
        return f"[{self.time}]\t{sum(c.value for c in self.coins)}$"

@dataclass
class Action(JSONWizard):
    step: int
    time: int
    amount: int
    operation: int
    fee: int
    num_coins: int
    coins: list[Coin]


def load_coins_json(file: Path) -> list[DataPoint]:
    with open(file, 'r') as json_file:
        return list(DataPoint.from_dict(d) for d in tqdm(json.load(json_file), desc=file.name))  

def load_actions_json(file) -> list[Action]:
    with open(file, 'r') as json_file:
        return list(Action.from_dict(d) for d in tqdm(json.load(json_file), desc=file.name))  

def load_file(file: Path):
    if file.name.startswith('coins'):
        yield load_coins_json(file)
    elif file.name.startswith('actions'):
        yield load_actions_json(file)
    else:
        print(f"{file.name} can not be read")

def load_all_files(path = PATH):
    files = list(f for f in Path.iterdir(path) if f.name.endswith(".json"))

    return {f: list(load_file(f)) for f in sorted(files)}

import re
massif_re = re.compile(r'mem_heap_B=(\d+)');

def _read_massif_peak(file_name):
    result = []
    with open(file_name) as file:
        for line in file.readlines():
            match = massif_re.match(line)
            if match:
                result.append(int(match.groups()[0]))
    return result


def load_massif(path=PATH):
    files = list(f for f in Path.iterdir(path) if f.name.endswith(".massif"))
    return {f: _read_massif_peak(f) for f in files}
    

