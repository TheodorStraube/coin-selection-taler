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
        return list(DataPoint.from_dict(d) for d in tqdm(json.load(json_file)))  

def load_actions_json(file) -> list[Action]:
    with open(file, 'r') as json_file:
        return list(Action.from_dict(d) for d in tqdm(json.load(json_file)))  

def load_file(file: Path):
    print("________________________________ " + file.name)
    if file.name.startswith('coins'):
        yield load_coins_json(file)
    elif file.name.startswith('actions'):
        yield load_actions_json(file)
    else:
        print(f"{file.name} can not be read")

def main():
    files = list(f for f in Path.iterdir(PATH) if f.name.endswith(".json"))

    return {f: list(load_file(f)) for f in files}


r = main()

