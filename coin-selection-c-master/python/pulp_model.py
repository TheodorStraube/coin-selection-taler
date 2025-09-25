from os import WNOHANG
from pulp import LpVariable, lpSum, LpProblem, LpMinimize, PULP_CBC_CMD
from pulp.constants import LpStatusOptimal

from dataclasses import dataclass, field


@dataclass
class Fees:
    deposit_fee: int
    refund_fee: int
    withdraw_fee: int
    refresh_fee: int


@dataclass
class Durations:
    legal: int
    deposit: int
    withdraw: int


@dataclass
class Rules:
    fees: Fees
    durations: Durations


@dataclass
class Denomination:
    name: str
    amount: int
    rules: Rules


@dataclass
class Coin:
    unique_id: int
    denomination: Denomination
    amount: int


@dataclass
class Wallet:
    coins: list[Coin]
    # global fees


SECONDS_IN_3_YEARS = 3 * 365 * 24 * 60 * 60


def parse_rules(rsa_keysize, cipher, fees, durations) -> Rules:
    return Rules(
        fees=Fees(*(satoshi for satoshi, percentage in fees)),
        durations=Durations(*durations),
    )


def parse_denomination(name, amount, rules_data) -> Denomination:
    return Denomination(name, amount, parse_rules(*rules_data))


def parse_coin(unique_id, denomination_data, creation_time, amount) -> Coin:
    return Coin(unique_id, parse_denomination(*denomination_data), amount)


def parse_wallet(wallet_data) -> Wallet:
    return Wallet(coins=[parse_coin(*coin_data) for coin_data in wallet_data["coins"]])


def select_minimize_fee(amount, wallet_data):
    wallet = parse_wallet(wallet_data)

    ncoins = len(wallet.coins)
    # print("ncoins", ncoins)

    # print("Amount", amount)
    # print(sum([c.denomination.amount for c in wallet.coins]))
    # print()

    selection = LpVariable.dicts("Selection", range(ncoins), cat="Binary")

    prob = LpProblem("CoinSelection", LpMinimize)

    # Objective Funtion
    prob += (
        lpSum(
            [
                coin.denomination.rules.fees.deposit_fee * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        ),
        "Denomination Fee",
    )

    # Constraints
    prob += (
        lpSum(
            [
                wallet.coins[i].denomination.amount - wallet.coins[i].denomination.rules.fees.deposit_fee * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        )
        >= amount,
        "Amount is covered",
    )
    solve_result = prob.solve(PULP_CBC_CMD(msg=False))

    if solve_result != LpStatusOptimal:
        return solve_result, []

    # amounts = [c.denomination.amount for c in wallet.coins]
    # print("Amounts: ", min(amounts), len(amounts), max(amounts))

    # Each of the variables is printed with it's resolved optimum value
    # for v in prob.variables():
    #    print(v.name, "=", v.varValue)
    selectedCoins = [k for k, v in selection.items() if v]

    # print("Spending: ", sum([coin.denomination.amount for i, coin in enumerate(wallet.coins) if i in selectedCoins]))
    # print("returning", [(i, wallet.coins[i].denomination.amount) for i, (k, v) in enumerate(selection.items()) if v])

    return solve_result, [i for i, v in selection.items() if v]


def select_minimize_deposit_refresh(amount, wallet_data, refresh_weight=0.5):
    wallet = parse_wallet(wallet_data)

    ncoins = len(wallet.coins)

    selection = LpVariable.dicts("Selection", range(ncoins), cat="Binary")

    prob = LpProblem("CoinSelection", LpMinimize)

    # Objective Funtion
    prob += (
        lpSum(
            [
                coin.denomination.rules.fees.deposit_fee * selection[i]
                + coin.denomination.rules.fees.refresh_fee
                * (1 - coin.denomination.rules.durations.legal / SECONDS_IN_3_YEARS)
                * (1 - selection[i])
                for i, coin in enumerate(wallet.coins)
            ]
        ),
        "Denomination Fee",
    )

    # Constraints
    prob += (
        lpSum(
            [
                (wallet.coins[i].denomination.amount + wallet.coins[i].denomination.rules.fees.deposit_fee) * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        )
        >= amount,
        "Amount is covered",
    )
    solve_result = prob.solve(PULP_CBC_CMD(msg=False))

    if solve_result != LpStatusOptimal:
        return solve_result, []

    # amounts = [c.denomination.amount for c in wallet.coins]
    # print("Amounts: ", min(amounts), len(amounts), max(amounts))

    # Each of the variables is printed with it's resolved optimum value
    # for v in prob.variables():
    #    print(v.name, "=", v.varValue)
    selectedCoins = [k for k, v in selection.items() if v]

    # print("Spending: ", sum([coin.denomination.amount for i, coin in enumerate(wallet.coins) if i in selectedCoins]))
    # print("returning", [(i, wallet.coins[i].denomination.amount) for i, (k, v) in enumerate(selection.items()) if v])

    return solve_result, [i for i, v in selection.items() if v]


def select_generous(amount, wallet_data):
    wallet = parse_wallet(wallet_data)

    ncoins = len(wallet.coins)

    selection = LpVariable.dicts("Selection", range(ncoins), cat="Binary")

    prob = LpProblem("CoinSelection", LpMinimize)

    # Objective Funtion
    prob += (
        lpSum(
            [
                -1 * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        ),
        "Denomination Fee",
    )

    # Constraints
    prob += (
        lpSum(
            [
                (wallet.coins[i].denomination.amount - wallet.coins[i].denomination.rules.fees.deposit_fee) * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        )
        >= amount,
        "Amount is covered",
    )
    solve_result = prob.solve(PULP_CBC_CMD(msg=False))

    if solve_result != LpStatusOptimal:
        return solve_result, []

    return solve_result, [i for i, v in selection.items() if v]


def select_closest(amount, wallet_data):
    wallet = parse_wallet(wallet_data)

    ncoins = len(wallet.coins)

    selection = LpVariable.dicts("Selection", range(ncoins), cat="Binary")
    ueberschreitung = LpVariable("ueberschreitung", lowBound=0)

    prob = LpProblem("CoinSelection", LpMinimize)

    # Objective Funtion
    prob += ueberschreitung

    # Constraints
    prob += (
        lpSum(
            [
                (wallet.coins[i].denomination.amount - wallet.coins[i].denomination.rules.fees.deposit_fee) * selection[i]
                for i, coin in enumerate(wallet.coins)
            ]
        )
        == amount + ueberschreitung,
        "Amount is covered",
    )
    solve_result = prob.solve(PULP_CBC_CMD(msg=False))

    if solve_result != LpStatusOptimal:
        return solve_result, []

    selectedCoins = [k for k, v in selection.items() if v]

    print("Spending: ", sum([coin.denomination.amount for i, coin in enumerate(wallet.coins) if i in selectedCoins]))
    print("returning", [(i, wallet.coins[i].denomination.amount) for i, (k, v) in enumerate(selection.items()) if v])

    return solve_result, [i for i, v in selection.items() if v]
