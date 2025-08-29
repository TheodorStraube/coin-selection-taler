from dataclasses import dataclass
from os import listdir
from typing import Generator
from dataclass_wizard import JSONWizard
from numpy import amax
from tqdm import tqdm
import json
from pathlib import Path
import pandas as pd
from itertools import chain
from pprint import pprint

PATH = Path("../coin-selection-c-master/simulation/results")
COIN_PX = 'coins_'
ACTION_PX = 'actions_'
ACTIONS = ["Deposit", "Withdraw", "Refund", "Refresh"]

@dataclass
class Coin:
    value: int
    denomination: int

@dataclass
class DataPoint(JSONWizard):
    time: int
    coins: list[Coin]

    def __repr__(self):
        return f"{self.time}\t{sum(c.value for c in self.coins)}"

@dataclass
class Action(JSONWizard):
    Id: int
    time: int
    amount: int
    operation: int
    fee: int
    CoinCount: int
    coins: list[Coin]

    def __repr__(self):
        return f"{self.time}\t{ACTIONS[self.operation]} {self.amount} fee: {self.fee}"


def load_coins_json(file: Path) -> list[DataPoint]:
    with open(file, 'r') as json_file:
        return list(DataPoint.from_dict(d) for d in tqdm(json.load(json_file), desc=file.name))  

def load_actions_json(file) -> list[Action]:
    with open(file, 'r') as json_file:
        return list(Action.from_dict(d) for d in tqdm(json.load(json_file), desc=file.name))  

def load_file(file: Path):
    if file.name.startswith('coins'):
        return load_coins_json(file)
    elif file.name.startswith('actions'):
        return load_actions_json(file)
    else:
        print(f"{file.name} can not be read")

def load_all_files(path = PATH):
    files = list(f for f in Path.iterdir(path) if f.name.endswith(".json"))

    return {f.name: load_file(f) for f in sorted(files)}

def load_df(actions, file_name):
    levels = [(0, 10), (10, 100), (100, 1000), (1000, 10000), (10000, 100000)]
    data = []
    for action in actions:
        level_data = {str((low, high)): len([c for c in action.coins if c.value < high and c.value >= low]) for low, high in levels}
        level_data["Time"] = action.time
        level_data["FileName"] = file_name
        data.append(level_data)
    return data

def load_all_dfs():
    files = list(f for f in Path.iterdir(PATH) if f.name.endswith(".json") and f.name.startswith('coins'))
    return pd.DataFrame(chain.from_iterable(load_df(load_coins_json(f), f.name) for f in sorted(files)))

def match_json_files():
    coin_files = list(f for f in Path.iterdir(PATH) if f.name.endswith(".json") and f.name.startswith(COIN_PX))
    action_files = list(f for f in Path.iterdir(PATH) if f.name.endswith(".json") and f.name.startswith(ACTION_PX))
    result = dict()

    for cf in coin_files:
        print(cf)
        result[cf.name.removeprefix(COIN_PX).removesuffix('.json')] = load_coins_json(cf)

    for af in action_files:
        print("key")
        key = af.name.removeprefix(ACTION_PX).removesuffix('.json')
        if key in result:
            result[key] = result[key], load_actions_json(af)
        else:
            print(f"Missing actions for {key}")

    return result

def validate_actions(coins: list[DataPoint], actions: list[Action]):
    expected_balance = 0
    last_balance = 0
    actual_balance = 0
    wallet = []

    for step in coins:
        next_wallet = list(step.coins)
        if wallet != next_wallet:
            last_actions = [a for a in actions if a.time == step.time]
            for action in last_actions:
                if action.operation == 0:
                    expected_balance -= action.amount + action.fee
                    print(f"{last_balance} -= {action.amount} + {action.fee} = {expected_balance}")

                elif action.operation == 1:
                    expected_balance += action.amount - action.fee
                    print(f"{last_balance} += {action.amount} - {action.fee} = {expected_balance}")
                else:
                    pass

            actual_balance = sum([c.value for c in next_wallet])
            if expected_balance != actual_balance: 
                print(f"{step.time} Balance ${last_balance} -> ${expected_balance} but is ${actual_balance}")
                pprint(last_actions)
                print(next_wallet)
                print()
        last_balance = actual_balance
        expected_balance = actual_balance
        wallet = next_wallet



def load_json_files():
    matches = match_json_files()
    return {k: (load_coins_json(c), load_actions_json(a)) for k, (c, a) in matches.items()}


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
    

